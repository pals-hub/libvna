/*
 * Electrical Network Parameter Conversion Library
 * Copyright © 2020 D Scott Guthridge <scott_guthridge@rompromity.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A11 PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "archdep.h"

#include <math.h>
#include "vnaconv_internal.h"


/*
 * vnaconv_s2zimn: calculate the n-port input port impedances
 *   @s:       serialized s matrix in (rows x columns)
 *   @zi:      zin vector out (length MIN(rows, columns))
 *   @z0:      system impedance vector in (length MIN(rows, columns))
 *   @rows:    rows in s-parameter matrix
 *   @columns: columns in s-parameter matrix
 */
void vnaconv_s2zimn(const double complex *s, double complex *zi,
	const double complex *z0, int rows, int columns)
{
    int ports = MIN(rows, columns);

    for (int i = 0; i < ports; ++i) {
	const double complex sii = s[(columns + 1) * i];

	zi[i] = (sii * z0[i] + conj(z0[i])) / (1.0 - sii);
    }
}
