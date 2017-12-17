/*
 * Bluetooth low-complexity, subband codec (SBC)
 *
 * Copyright (C) 2017  Aurelien Jacobs <aurel@gnuage.org>
 * Copyright (C) 2008-2010  Nokia Corporation
 * Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 * Copyright (C) 2004-2005  Henryk Ploetz <henryk@ploetzli.ch>
 * Copyright (C) 2005-2006  Brad Midgley <bmidgley@xmission.com>
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

/**
 * @file
 * miscellaneous SBC tables
 */

#include "sbcdsp_data.h"

#define F_PROTO(x) ((int32_t) (((x) * 2) * ((int32_t) 1 << 15) + 0.5))
#define F_COS(x)   ((int32_t) (((x)    ) * ((int32_t) 1 << 15) + 0.5))

/*
 * Constant tables for the use in SIMD optimized analysis filters
 * Each table consists of two parts:
 * 1. reordered "proto" table
 * 2. reordered "cos" table
 *
 * Due to non-symmetrical reordering, separate tables for "even"
 * and "odd" cases are needed
 */

DECLARE_ALIGNED(SBC_ALIGN, const int16_t, ff_sbcdsp_analysis_consts_fixed4_simd_even)[40 + 16] = {
#define C0 1.0932568993
#define C1 1.3056875580
#define C2 1.3056875580
#define C3 1.6772280856

#define F(x) F_PROTO(x)
     F(0.00000000E+00 * C0),  F(3.83720193E-03 * C0),
     F(5.36548976E-04 * C1),  F(2.73370904E-03 * C1),
     F(3.06012286E-03 * C2),  F(3.89205149E-03 * C2),
     F(0.00000000E+00 * C3), -F(1.49188357E-03 * C3),
     F(1.09137620E-02 * C0),  F(2.58767811E-02 * C0),
     F(2.04385087E-02 * C1),  F(3.21939290E-02 * C1),
     F(7.76463494E-02 * C2),  F(6.13245186E-03 * C2),
     F(0.00000000E+00 * C3), -F(2.88757392E-02 * C3),
     F(1.35593274E-01 * C0),  F(2.94315332E-01 * C0),
     F(1.94987841E-01 * C1),  F(2.81828203E-01 * C1),
    -F(1.94987841E-01 * C2),  F(2.81828203E-01 * C2),
     F(0.00000000E+00 * C3), -F(2.46636662E-01 * C3),
    -F(1.35593274E-01 * C0),  F(2.58767811E-02 * C0),
    -F(7.76463494E-02 * C1),  F(6.13245186E-03 * C1),
    -F(2.04385087E-02 * C2),  F(3.21939290E-02 * C2),
     F(0.00000000E+00 * C3),  F(2.88217274E-02 * C3),
    -F(1.09137620E-02 * C0),  F(3.83720193E-03 * C0),
    -F(3.06012286E-03 * C1),  F(3.89205149E-03 * C1),
    -F(5.36548976E-04 * C2),  F(2.73370904E-03 * C2),
     F(0.00000000E+00 * C3), -F(1.86581691E-03 * C3),
#undef F
#define F(x) F_COS(x)
     F(0.7071067812 / C0),  F(0.9238795325 / C1),
    -F(0.7071067812 / C0),  F(0.3826834324 / C1),
    -F(0.7071067812 / C0), -F(0.3826834324 / C1),
     F(0.7071067812 / C0), -F(0.9238795325 / C1),
     F(0.3826834324 / C2), -F(1.0000000000 / C3),
    -F(0.9238795325 / C2), -F(1.0000000000 / C3),
     F(0.9238795325 / C2), -F(1.0000000000 / C3),
    -F(0.3826834324 / C2), -F(1.0000000000 / C3),
#undef F

#undef C0
#undef C1
#undef C2
#undef C3
};

DECLARE_ALIGNED(SBC_ALIGN, const int16_t, ff_sbcdsp_analysis_consts_fixed4_simd_odd)[40 + 16] = {
#define C0 1.3056875580
#define C1 1.6772280856
#define C2 1.0932568993
#define C3 1.3056875580

#define F(x) F_PROTO(x)
     F(2.73370904E-03 * C0),  F(5.36548976E-04 * C0),
    -F(1.49188357E-03 * C1),  F(0.00000000E+00 * C1),
     F(3.83720193E-03 * C2),  F(1.09137620E-02 * C2),
     F(3.89205149E-03 * C3),  F(3.06012286E-03 * C3),
     F(3.21939290E-02 * C0),  F(2.04385087E-02 * C0),
    -F(2.88757392E-02 * C1),  F(0.00000000E+00 * C1),
     F(2.58767811E-02 * C2),  F(1.35593274E-01 * C2),
     F(6.13245186E-03 * C3),  F(7.76463494E-02 * C3),
     F(2.81828203E-01 * C0),  F(1.94987841E-01 * C0),
    -F(2.46636662E-01 * C1),  F(0.00000000E+00 * C1),
     F(2.94315332E-01 * C2), -F(1.35593274E-01 * C2),
     F(2.81828203E-01 * C3), -F(1.94987841E-01 * C3),
     F(6.13245186E-03 * C0), -F(7.76463494E-02 * C0),
     F(2.88217274E-02 * C1),  F(0.00000000E+00 * C1),
     F(2.58767811E-02 * C2), -F(1.09137620E-02 * C2),
     F(3.21939290E-02 * C3), -F(2.04385087E-02 * C3),
     F(3.89205149E-03 * C0), -F(3.06012286E-03 * C0),
    -F(1.86581691E-03 * C1),  F(0.00000000E+00 * C1),
     F(3.83720193E-03 * C2),  F(0.00000000E+00 * C2),
     F(2.73370904E-03 * C3), -F(5.36548976E-04 * C3),
#undef F
#define F(x) F_COS(x)
     F(0.9238795325 / C0), -F(1.0000000000 / C1),
     F(0.3826834324 / C0), -F(1.0000000000 / C1),
    -F(0.3826834324 / C0), -F(1.0000000000 / C1),
    -F(0.9238795325 / C0), -F(1.0000000000 / C1),
     F(0.7071067812 / C2),  F(0.3826834324 / C3),
    -F(0.7071067812 / C2), -F(0.9238795325 / C3),
    -F(0.7071067812 / C2),  F(0.9238795325 / C3),
     F(0.7071067812 / C2), -F(0.3826834324 / C3),
#undef F

#undef C0
#undef C1
#undef C2
#undef C3
};

DECLARE_ALIGNED(SBC_ALIGN, const int16_t, ff_sbcdsp_analysis_consts_fixed8_simd_even)[80 + 64] = {
#define C0 2.7906148894
#define C1 2.4270044280
#define C2 2.8015616024
#define C3 3.1710363741
#define C4 2.5377944043
#define C5 2.4270044280
#define C6 2.8015616024
#define C7 3.1710363741

#define F(x) F_PROTO(x)
     F(0.00000000E+00 * C0),  F(2.01182542E-03 * C0),
     F(1.56575398E-04 * C1),  F(1.78371725E-03 * C1),
     F(3.43256425E-04 * C2),  F(1.47640169E-03 * C2),
     F(5.54620202E-04 * C3),  F(1.13992507E-03 * C3),
    -F(8.23919506E-04 * C4),  F(0.00000000E+00 * C4),
     F(2.10371989E-03 * C5),  F(3.49717454E-03 * C5),
     F(1.99454554E-03 * C6),  F(1.64973098E-03 * C6),
     F(1.61656283E-03 * C7),  F(1.78805361E-04 * C7),
     F(5.65949473E-03 * C0),  F(1.29371806E-02 * C0),
     F(8.02941163E-03 * C1),  F(1.53184106E-02 * C1),
     F(1.04584443E-02 * C2),  F(1.62208471E-02 * C2),
     F(1.27472335E-02 * C3),  F(1.59045603E-02 * C3),
    -F(1.46525263E-02 * C4),  F(0.00000000E+00 * C4),
     F(8.85757540E-03 * C5),  F(5.31873032E-02 * C5),
     F(2.92408442E-03 * C6),  F(3.90751381E-02 * C6),
    -F(4.91578024E-03 * C7),  F(2.61098752E-02 * C7),
     F(6.79989431E-02 * C0),  F(1.46955068E-01 * C0),
     F(8.29847578E-02 * C1),  F(1.45389847E-01 * C1),
     F(9.75753918E-02 * C2),  F(1.40753505E-01 * C2),
     F(1.11196689E-01 * C3),  F(1.33264415E-01 * C3),
    -F(1.23264548E-01 * C4),  F(0.00000000E+00 * C4),
     F(1.45389847E-01 * C5), -F(8.29847578E-02 * C5),
     F(1.40753505E-01 * C6), -F(9.75753918E-02 * C6),
     F(1.33264415E-01 * C7), -F(1.11196689E-01 * C7),
    -F(6.79989431E-02 * C0),  F(1.29371806E-02 * C0),
    -F(5.31873032E-02 * C1),  F(8.85757540E-03 * C1),
    -F(3.90751381E-02 * C2),  F(2.92408442E-03 * C2),
    -F(2.61098752E-02 * C3), -F(4.91578024E-03 * C3),
     F(1.46404076E-02 * C4),  F(0.00000000E+00 * C4),
     F(1.53184106E-02 * C5), -F(8.02941163E-03 * C5),
     F(1.62208471E-02 * C6), -F(1.04584443E-02 * C6),
     F(1.59045603E-02 * C7), -F(1.27472335E-02 * C7),
    -F(5.65949473E-03 * C0),  F(2.01182542E-03 * C0),
    -F(3.49717454E-03 * C1),  F(2.10371989E-03 * C1),
    -F(1.64973098E-03 * C2),  F(1.99454554E-03 * C2),
    -F(1.78805361E-04 * C3),  F(1.61656283E-03 * C3),
    -F(9.02154502E-04 * C4),  F(0.00000000E+00 * C4),
     F(1.78371725E-03 * C5), -F(1.56575398E-04 * C5),
     F(1.47640169E-03 * C6), -F(3.43256425E-04 * C6),
     F(1.13992507E-03 * C7), -F(5.54620202E-04 * C7),
#undef F
#define F(x) F_COS(x)
     F(0.7071067812 / C0),  F(0.8314696123 / C1),
    -F(0.7071067812 / C0), -F(0.1950903220 / C1),
    -F(0.7071067812 / C0), -F(0.9807852804 / C1),
     F(0.7071067812 / C0), -F(0.5555702330 / C1),
     F(0.7071067812 / C0),  F(0.5555702330 / C1),
    -F(0.7071067812 / C0),  F(0.9807852804 / C1),
    -F(0.7071067812 / C0),  F(0.1950903220 / C1),
     F(0.7071067812 / C0), -F(0.8314696123 / C1),
     F(0.9238795325 / C2),  F(0.9807852804 / C3),
     F(0.3826834324 / C2),  F(0.8314696123 / C3),
    -F(0.3826834324 / C2),  F(0.5555702330 / C3),
    -F(0.9238795325 / C2),  F(0.1950903220 / C3),
    -F(0.9238795325 / C2), -F(0.1950903220 / C3),
    -F(0.3826834324 / C2), -F(0.5555702330 / C3),
     F(0.3826834324 / C2), -F(0.8314696123 / C3),
     F(0.9238795325 / C2), -F(0.9807852804 / C3),
    -F(1.0000000000 / C4),  F(0.5555702330 / C5),
    -F(1.0000000000 / C4), -F(0.9807852804 / C5),
    -F(1.0000000000 / C4),  F(0.1950903220 / C5),
    -F(1.0000000000 / C4),  F(0.8314696123 / C5),
    -F(1.0000000000 / C4), -F(0.8314696123 / C5),
    -F(1.0000000000 / C4), -F(0.1950903220 / C5),
    -F(1.0000000000 / C4),  F(0.9807852804 / C5),
    -F(1.0000000000 / C4), -F(0.5555702330 / C5),
     F(0.3826834324 / C6),  F(0.1950903220 / C7),
    -F(0.9238795325 / C6), -F(0.5555702330 / C7),
     F(0.9238795325 / C6),  F(0.8314696123 / C7),
    -F(0.3826834324 / C6), -F(0.9807852804 / C7),
    -F(0.3826834324 / C6),  F(0.9807852804 / C7),
     F(0.9238795325 / C6), -F(0.8314696123 / C7),
    -F(0.9238795325 / C6),  F(0.5555702330 / C7),
     F(0.3826834324 / C6), -F(0.1950903220 / C7),
#undef F

#undef C0
#undef C1
#undef C2
#undef C3
#undef C4
#undef C5
#undef C6
#undef C7
};

DECLARE_ALIGNED(SBC_ALIGN, const int16_t, ff_sbcdsp_analysis_consts_fixed8_simd_odd)[80 + 64] = {
#define C0 2.5377944043
#define C1 2.4270044280
#define C2 2.8015616024
#define C3 3.1710363741
#define C4 2.7906148894
#define C5 2.4270044280
#define C6 2.8015616024
#define C7 3.1710363741

#define F(x) F_PROTO(x)
     F(0.00000000E+00 * C0), -F(8.23919506E-04 * C0),
     F(1.56575398E-04 * C1),  F(1.78371725E-03 * C1),
     F(3.43256425E-04 * C2),  F(1.47640169E-03 * C2),
     F(5.54620202E-04 * C3),  F(1.13992507E-03 * C3),
     F(2.01182542E-03 * C4),  F(5.65949473E-03 * C4),
     F(2.10371989E-03 * C5),  F(3.49717454E-03 * C5),
     F(1.99454554E-03 * C6),  F(1.64973098E-03 * C6),
     F(1.61656283E-03 * C7),  F(1.78805361E-04 * C7),
     F(0.00000000E+00 * C0), -F(1.46525263E-02 * C0),
     F(8.02941163E-03 * C1),  F(1.53184106E-02 * C1),
     F(1.04584443E-02 * C2),  F(1.62208471E-02 * C2),
     F(1.27472335E-02 * C3),  F(1.59045603E-02 * C3),
     F(1.29371806E-02 * C4),  F(6.79989431E-02 * C4),
     F(8.85757540E-03 * C5),  F(5.31873032E-02 * C5),
     F(2.92408442E-03 * C6),  F(3.90751381E-02 * C6),
    -F(4.91578024E-03 * C7),  F(2.61098752E-02 * C7),
     F(0.00000000E+00 * C0), -F(1.23264548E-01 * C0),
     F(8.29847578E-02 * C1),  F(1.45389847E-01 * C1),
     F(9.75753918E-02 * C2),  F(1.40753505E-01 * C2),
     F(1.11196689E-01 * C3),  F(1.33264415E-01 * C3),
     F(1.46955068E-01 * C4), -F(6.79989431E-02 * C4),
     F(1.45389847E-01 * C5), -F(8.29847578E-02 * C5),
     F(1.40753505E-01 * C6), -F(9.75753918E-02 * C6),
     F(1.33264415E-01 * C7), -F(1.11196689E-01 * C7),
     F(0.00000000E+00 * C0),  F(1.46404076E-02 * C0),
    -F(5.31873032E-02 * C1),  F(8.85757540E-03 * C1),
    -F(3.90751381E-02 * C2),  F(2.92408442E-03 * C2),
    -F(2.61098752E-02 * C3), -F(4.91578024E-03 * C3),
     F(1.29371806E-02 * C4), -F(5.65949473E-03 * C4),
     F(1.53184106E-02 * C5), -F(8.02941163E-03 * C5),
     F(1.62208471E-02 * C6), -F(1.04584443E-02 * C6),
     F(1.59045603E-02 * C7), -F(1.27472335E-02 * C7),
     F(0.00000000E+00 * C0), -F(9.02154502E-04 * C0),
    -F(3.49717454E-03 * C1),  F(2.10371989E-03 * C1),
    -F(1.64973098E-03 * C2),  F(1.99454554E-03 * C2),
    -F(1.78805361E-04 * C3),  F(1.61656283E-03 * C3),
     F(2.01182542E-03 * C4),  F(0.00000000E+00 * C4),
     F(1.78371725E-03 * C5), -F(1.56575398E-04 * C5),
     F(1.47640169E-03 * C6), -F(3.43256425E-04 * C6),
     F(1.13992507E-03 * C7), -F(5.54620202E-04 * C7),
#undef F
#define F(x) F_COS(x)
    -F(1.0000000000 / C0),  F(0.8314696123 / C1),
    -F(1.0000000000 / C0), -F(0.1950903220 / C1),
    -F(1.0000000000 / C0), -F(0.9807852804 / C1),
    -F(1.0000000000 / C0), -F(0.5555702330 / C1),
    -F(1.0000000000 / C0),  F(0.5555702330 / C1),
    -F(1.0000000000 / C0),  F(0.9807852804 / C1),
    -F(1.0000000000 / C0),  F(0.1950903220 / C1),
    -F(1.0000000000 / C0), -F(0.8314696123 / C1),
     F(0.9238795325 / C2),  F(0.9807852804 / C3),
     F(0.3826834324 / C2),  F(0.8314696123 / C3),
    -F(0.3826834324 / C2),  F(0.5555702330 / C3),
    -F(0.9238795325 / C2),  F(0.1950903220 / C3),
    -F(0.9238795325 / C2), -F(0.1950903220 / C3),
    -F(0.3826834324 / C2), -F(0.5555702330 / C3),
     F(0.3826834324 / C2), -F(0.8314696123 / C3),
     F(0.9238795325 / C2), -F(0.9807852804 / C3),
     F(0.7071067812 / C4),  F(0.5555702330 / C5),
    -F(0.7071067812 / C4), -F(0.9807852804 / C5),
    -F(0.7071067812 / C4),  F(0.1950903220 / C5),
     F(0.7071067812 / C4),  F(0.8314696123 / C5),
     F(0.7071067812 / C4), -F(0.8314696123 / C5),
    -F(0.7071067812 / C4), -F(0.1950903220 / C5),
    -F(0.7071067812 / C4),  F(0.9807852804 / C5),
     F(0.7071067812 / C4), -F(0.5555702330 / C5),
     F(0.3826834324 / C6),  F(0.1950903220 / C7),
    -F(0.9238795325 / C6), -F(0.5555702330 / C7),
     F(0.9238795325 / C6),  F(0.8314696123 / C7),
    -F(0.3826834324 / C6), -F(0.9807852804 / C7),
    -F(0.3826834324 / C6),  F(0.9807852804 / C7),
     F(0.9238795325 / C6), -F(0.8314696123 / C7),
    -F(0.9238795325 / C6),  F(0.5555702330 / C7),
     F(0.3826834324 / C6), -F(0.1950903220 / C7),
#undef F

#undef C0
#undef C1
#undef C2
#undef C3
#undef C4
#undef C5
#undef C6
#undef C7
};
