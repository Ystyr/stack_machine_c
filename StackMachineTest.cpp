#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <vector>
#include <string>
#include <unordered_map>

#define STACK_SIZE 128
#define LITTLE_ENDIAN 1

/// <summary>
/// time, torque, sensor values etc. to be set externally
/// </summary>
float BuiltInVars[256];

typedef struct {
    float stack[STACK_SIZE];
    int top;
} Stack;

void initializeStack(Stack* stack) {
    stack->top = -1;
}

int isStackEmpty(Stack* stack) {
    return stack->top == -1;
}

int isStackFull(Stack* stack) {
    return stack->top == STACK_SIZE - 1;
}

void push(Stack* stack, float value) {
    if (isStackFull(stack)) {
        printf("Stack overflow!\n");
        exit(1);
    }

    stack->stack[++stack->top] = value;
}

float pop(Stack* stack) {
    if (isStackEmpty(stack)) {
        printf("Stack underflow!\n");
        exit(1);
    }

    float value = stack->stack[stack->top];
    stack->top--;
    return value;
}

float bytesToFloat(const uint8_t* bytes) {
    union {
        uint8_t byteData[sizeof(float)];
        float floatValue;
    } data;

    if (LITTLE_ENDIAN) {
        for (int i = 0; i < sizeof(float); i++) {
            data.byteData[i] = bytes[i];
        }
    }
    else {
        for (int i = sizeof(float) - 1; i >= 0; i--) {
            data.byteData[i] = bytes[sizeof(float) - 1 - i];
        }
    }

    return data.floatValue;
}

void floatToBytes(float value, uint8_t* bytes) {
    union {
        float floatValue;
        uint8_t byteData[sizeof(float)];
    } data;

    data.floatValue = value;

    if (LITTLE_ENDIAN) {
        for (int i = 0; i < sizeof(float); i++) {
            bytes[i] = data.byteData[i];
        }
    }
    else {
        for (int i = sizeof(float) - 1; i >= 0; i--) {
            bytes[sizeof(float) - 1 - i] = data.byteData[i];
        }
    }
}

void buildByteSequence(const char* expression, uint8_t** byteSequence, int* sequenceLength)
{
    const char delimiters[] = " ,";
    const char opcodes[] = "+-*/<>%sctp";

    const int numOperators = strlen(opcodes);

    size_t bufferSize = strlen(expression) + 1;
    char* expressionCopy = (char*)malloc(bufferSize);

    strncpy_s(expressionCopy, bufferSize, expression, bufferSize);

    // Tokenize the expression
    char* token = NULL;
    char* context = NULL;
    token = strtok_s(expressionCopy, delimiters, &context);

    std::vector<uint8_t> byteSequenceVec;

    while (token != NULL) {
        float value;
        if (sscanf_s(token, "%f", &value) == 1) {
            byteSequenceVec.push_back('#');
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
            for (int i = 0; i < sizeof(float); i++) {
                byteSequenceVec.push_back(bytes[i]);
            }
        }
        else {
            if (token[0] == '$') {
                byteSequenceVec.push_back(token[0]);
                uint8_t varIndex = static_cast<uint8_t>(atoi(token + 1));
                byteSequenceVec.push_back(varIndex);
            }
            else {
                bool foundOperator = false;
                std::string tokenStr(token);
                for (int i = 0; i < numOperators; i++) {
                    if (tokenStr == std::string(1, opcodes[i])) {
                        byteSequenceVec.push_back(opcodes[i]);
                        foundOperator = true;
                        break;
                    }
                }
                if (!foundOperator) {
                    printf("Invalid operator: %s\n", token);
                    exit(1);
                }
            }
        }

        token = strtok_s(NULL, delimiters, &context);
    }

    *sequenceLength = byteSequenceVec.size();
    *byteSequence = (uint8_t*)malloc(*sequenceLength);
    memcpy(*byteSequence, byteSequenceVec.data(), *sequenceLength);

    free(expressionCopy);
}

float evaluateExpression (const uint8_t* sequence, int length) 
{
    Stack stack;
    initializeStack(&stack);

    int i = 0;
    while (i < length) {
        uint8_t opcode = sequence[i++];

        switch (opcode) {
        // read literal
        case '#': {
            uint8_t bytes[sizeof(float)];
            for (int j = 0; j < sizeof(float); j++) {;
                bytes[j] = sequence[i++];
            }
            float value = bytesToFloat(bytes);
            push(&stack, value);

            break;
        }
        // push built-in var
        case '$': {
            int idx = sequence[i++];
            float builtIn = BuiltInVars[idx];
            push(&stack, builtIn);
            break;
        }
        // math operators
        case '+': {
            float op2 = pop(&stack);
            float op1 = pop(&stack);
            push(&stack, op1 + op2);
            break;
        }
        case '-': {
            float op2 = pop(&stack);
            float op1 = pop(&stack);
            push(&stack, op1 - op2);
            break;
        }
        case '*': {
            float op2 = pop(&stack);
            float op1 = pop(&stack);
            push(&stack, op1 * op2);
            break;
        }
        case '/': {
            float op2 = pop(&stack);
            float op1 = pop(&stack);
            push(&stack, op1 / op2);
            break;
        }
        case '%': {
            float op2 = pop(&stack);
            float op1 = pop(&stack);
            push(&stack,fmod(op1, op2));
            break;
        }
        case '<': {
            float op2 = pop(&stack);
            float op1 = pop(&stack);
            push(&stack, op1 < op2);
            break;
        }
        case '>': {
            float op2 = pop(&stack);
            float op1 = pop(&stack);
            push(&stack, op1 > op2);
            break;
        }
        case 'p': {
            float op2 = pop(&stack);
            float op1 = pop(&stack);
            push(&stack, pow(op1, op2));
            break;
        }
        case 's': {
            float value = pop(&stack);
            push(&stack, sin(value));
            break;
        }
        case 'c': {
            float value = pop(&stack);
            push(&stack, cos(value));
            break;
        }
        case 't': {
            float value = pop(&stack);
            push(&stack, tan(value));
            break;
        }
        }
    }

    return pop(&stack);
}

void testExpressions()
{
    struct ExpressionTest
    {
        const char* expression;
        float expectedResult;
    };

    ExpressionTest tests[] = {
        {"1, 2, 3, 4, *, +, -", 1 - (2 + 3 * 4)},
        {"10, 5, /, 2, 3, *, +", (10 / 5) + 2 * 3},
        {"2, 3, 4, *, 5, /, +", 2 + (3 * 4) / 5.0},
        {"3.14, 2, *, s, 1.2, 3.2, *, c, -", sin(3.14 * 2.0) - cos(1.2 * 3.2)},
        {"2.5, s, 1.2, c, +, 0.8, t, -", sin(2.5) + cos(1.2) - tan(0.8)},
        {"$0, 2, *, s, 7, p", pow(sin(BuiltInVars[0] * 2), 7)}
    };

    const int numTests = sizeof(tests) / sizeof(tests[0]);
    const float tolerance = 0.0001f;

    for (int i = 0; i < numTests; i++)
    {
        const char* expression = tests[i].expression;
        float expectedResult = tests[i].expectedResult;

        uint8_t* byteSequence;
        int sequenceLength;

        buildByteSequence(expression, &byteSequence, &sequenceLength);

        printf("Expression: %s\n", expression);
        printf("Byte Sequence: ");
        for (int j = 0; j < sequenceLength; j++) {
            printf("%u ", byteSequence[j]);
        }
        printf("\n");

        float result = evaluateExpression(byteSequence, sequenceLength);

        printf("Result: %f\n", result);
        printf("Expected Result: %f\n", expectedResult);

        if (fabs(result - expectedResult) < tolerance) {
            printf("Test Passed!\n");
        }
        else {
            printf("Test Failed!\n");
        }

        printf("----------------------------\n");

        free(byteSequence);
    }
}


int main () 
{
    BuiltInVars[0] = 4.1;
    testExpressions();

    return 0;
}