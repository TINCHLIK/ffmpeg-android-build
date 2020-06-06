/*
 * Copyright (c) 2020
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "libavfilter/dnn/dnn_backend_native_layer_mathunary.h"
#include "libavutil/avassert.h"

#define EPS 0.00001

static float get_expected(float f, DNNMathUnaryOperation op)
{
    switch (op)
    {
    case DMUO_ABS:
        return (f >= 0) ? f : -f;
    case DMUO_SIN:
        return sin(f);
    case DMUO_COS:
        return cos(f);
    case DMUO_TAN:
        return tan(f);
    default:
        av_assert0(!"not supported yet");
        return 0.f;
    }
}

static int test(DNNMathUnaryOperation op)
{
    DnnLayerMathUnaryParams params;
    DnnOperand operands[2];
    int32_t input_indexes[1];
    float input[1*1*2*3] = {
        -3, 2.5, 2, -2.1, 7.8, 100};
    float *output;

    params.un_op = op;

    operands[0].data = input;
    operands[0].dims[0] = 1;
    operands[0].dims[1] = 1;
    operands[0].dims[2] = 2;
    operands[0].dims[3] = 3;
    operands[1].data = NULL;

    input_indexes[0] = 0;
    dnn_execute_layer_math_unary(operands, input_indexes, 1, &params);

    output = operands[1].data;
    for (int i = 0; i < sizeof(input) / sizeof(float); ++i) {
        float expected_output = get_expected(input[i], op);
        if(fabs(output[i] - expected_output) > EPS) {
            printf("at index %d, output: %f, expected_output: %f\n", i, output[i], expected_output);
            av_freep(&output);
            return 1;
        }
    }

    av_freep(&output);
    return 0;
}

int main(int agrc, char **argv)
{
    if (test(DMUO_ABS))
        return 1;
    if (test(DMUO_SIN))
        return 1;
    if (test(DMUO_COS))
        return 1;
    if (test(DMUO_TAN))
        return 1;
    return 0;
}
