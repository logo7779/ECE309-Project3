// src/tools/calculator_eval.cpp — iterative two-stack expression evaluator.
#include "tools/calculator.h"

#include <vector>

namespace {
using Kind = CalcToken::Kind;

int precedence(Kind kind) { //defien priority of operations via PEMDAS ()
    if (kind == Kind::Times || kind == Kind::Divide) return 2;
    if (kind == Kind::Plus || kind == Kind::Minus) return 1;
    return 0;
}

bool apply_top(std::vector<double>* values, std::vector<Kind>* ops, std::string* error) {
    //an invalid operation causes stack to be discarded and throws error meessage as to why
    if (ops->empty() || values->size() < 2) {//make certain an operator and 2 values exists (i.e. 2+ doesnt make sense)
        *error = "malformed expression"; //Missing operand or value, so expression is missing something
        return false;
    }
    const Kind op = ops->back();//assign operator to op
    ops->pop_back();
    const double right = values->back();//assign right value
    values->pop_back();
    const double left = values->back();//assign left value
    values->pop_back();

    double result = 0.0;
    switch (op) { // perform operations based on priority and the assigned op/values from above
        case Kind::Plus:   result = left + right; break;
        case Kind::Minus:  result = left - right; break;
        case Kind::Times:  result = left * right; break;
        case Kind::Divide:
            if (right == 0.0) {// check for divide by zero
                *error = "division by zero";
                return false;
            }
            result = left / right; //else divide
            break;
        default: // incase a non-basic symbol is entered
            *error = "malformed expression"; //catch any missed but wrong input
            return false;
    }
    values->push_back(result); //push result to the stack
    return true; //let caller know the operation was successful
}
}

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out, std::string* error) {
    std::vector<double> values;
    std::vector<Kind> ops;
    error->clear(); // remove old errors before continuing
    if (tokens.empty()) {
        *error = "empty expression";
        return false;
    }

    for (const CalcToken& token : tokens) { //process every token on stack
        switch (token.kind) {
            case Kind::Number:
                values.push_back(token.value);
                break;
            case Kind::LParen:
                ops.push_back(Kind::LParen);
                break;
            case Kind::RParen:
                while (!ops.empty() && ops.back() != Kind::LParen) {
                    if (!apply_top(&values, &ops, error)) return false;
                }
                if (ops.empty()) {
                    *error = "malformed expression";
                    return false;
                }
                ops.pop_back();
                break;
            case Kind::Plus:
            case Kind::Minus:
            case Kind::Times:
            case Kind::Divide:
                while (!ops.empty() && ops.back() != Kind::LParen &&        //go through operations
                       precedence(ops.back()) >= precedence(token.kind)) { // equal operators evaluate left to right (PEMDAS)
                    if (!apply_top(&values, &ops, error)) return false;
                }
                ops.push_back(token.kind);
                break;
            default:
                *error = "malformed expression";
                return false;
        }
    }

    while (!ops.empty()) { //finish remaining operations
        if (ops.back() == Kind::LParen) {
            *error = "malformed expression";
            return false;
        }
        if (!apply_top(&values, &ops, error)) return false;
    }
    if (values.size() != 1) { // after completion, only final answer should remain on stack
        *error = "malformed expression";
        return false;
    }
    *out = values.back();
    return true; // report success completion of the operation
}
