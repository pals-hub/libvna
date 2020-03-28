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

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "vnadata_internal.h"


/*
 * _vnadata_validate: validate the dimensions vs. type
 *   @rows: number of rows in the parameter matrix
 *   @columns: number of columns in the parameter matrix
 *   @type: parameter type
 */
static int _vnadata_validate(int rows, int columns,
	vnadata_parameter_type_t type)
{
    switch (type) {
    /* generic */
    case VPT_UNDEF:
    case VPT_S:
	break;

    /* square only */
    case VPT_Z:
    case VPT_Y:
	if (rows != columns) {
	    errno = EINVAL;
	    return -1;
	}
	break;

    /* two-port only */
    case VPT_T:
    case VPT_H:
    case VPT_G:
    case VPT_A:
    case VPT_B:
	if (rows != 2 || columns != 2) {
	    errno = EINVAL;
	    return -1;
	}

    /* vector only */
    case VPT_ZIN:
	if (rows != 1 && columns != 1) {
	    errno = EINVAL;
	    return -1;
	}
	break;

    default:
	errno = EINVAL;
	return -1;
    }
    return 0;
}

/*
 * _vnadata_extend_p: extend the port allocation for Z0
 *   @vdip: internal object pointer
 *   @new_p_allocation: new allocation
 */
int _vnadata_extend_p(vnadata_internal_t *vdip, int new_p_allocation)
{
    int old_p_allocation = vdip->vdi_p_allocation;

    if (new_p_allocation > old_p_allocation) {
	if (vdip->vdi_flags & VF_PER_F_Z0) {
	    for (int findex = 0; findex < vdip->vdi_f_allocation; ++findex) {
		double complex *clfp;

		clfp = realloc(vdip->vdi_z0_vector_vector[findex],
			new_p_allocation * sizeof(double complex));
		if (clfp == NULL) {
		    return -1;
		}
		for (int i = old_p_allocation; i < new_p_allocation; ++i) {
		    clfp[i] = VNADATA_DEFAULT_Z0;
		}
		vdip->vdi_z0_vector_vector[findex] = clfp;
	    }
	} else {
	    double complex *clfp;

	    clfp = realloc(vdip->vdi_z0_vector,
		    new_p_allocation * sizeof(double complex));
	    if (clfp == NULL) {
		return -1;
	    }
	    for (int i = old_p_allocation; i < new_p_allocation; ++i) {
		clfp[i] = VNADATA_DEFAULT_Z0;
	    }
	    vdip->vdi_z0_vector = clfp;
	}
	vdip->vdi_p_allocation = new_p_allocation;
    }
    return 0;
}

/*
 * _vnadata_extend_m: extend the matrix allocation
 *   @vdip: internal object pointer
 *   @new_m_allocation: new allocation
 */
int _vnadata_extend_m(vnadata_internal_t *vdip, int new_m_allocation)
{
    vnadata_t *vdp = &vdip->vdi_vd;
    int old_m_allocation = vdip->vdi_m_allocation;

    if (new_m_allocation > old_m_allocation) {
	for (int findex = 0; findex < vdip->vdi_f_allocation; ++findex) {
	    double complex *clfp;

	    if ((clfp = realloc(vdp->vd_data[findex], new_m_allocation *
			    sizeof(double complex))) == NULL) {
		return -1;
	    }
	    (void)memset((void *)&clfp[old_m_allocation], 0,
		    (new_m_allocation - old_m_allocation) *
		    sizeof(double complex));
	    vdp->vd_data[findex] = clfp;
	}
	vdip->vdi_m_allocation = new_m_allocation;
    }
    return 0;
}

/*
 * _vnadata_extend_f: extend the frequency allocation
 *   @vdip: internal object pointer
 *   @new_f_allocation: new allocation
 */
int _vnadata_extend_f(vnadata_internal_t *vdip, int new_f_allocation)
{
    vnadata_t *vdp = &vdip->vdi_vd;
    int old_f_allocation = vdip->vdi_f_allocation;

    if (new_f_allocation > old_f_allocation) {
	double *lfp;
	double complex **clfpp;

	/*
	 * Extend the frequency vector.
	 */
	lfp = realloc(vdp->vd_frequency_vector,
		new_f_allocation * sizeof(double));
	if (lfp == NULL) {
	    return -1;
	}
	(void)memset((void *)&lfp[old_f_allocation], 0,
		(new_f_allocation - old_f_allocation) * sizeof(double));
	vdp->vd_frequency_vector = lfp;

	/*
	 * If per-frequency Z0, extend the Z0 vector vector.
	 */
	if (vdip->vdi_flags & VF_PER_F_Z0) {
	    if ((clfpp = realloc(vdip->vdi_z0_vector_vector, new_f_allocation *
			    sizeof(double complex *))) == NULL) {
		return -1;
	    }
	    vdip->vdi_z0_vector_vector = clfpp;
	}

	/*
	 * Extend vd_data.
	 */
	clfpp = realloc(vdp->vd_data, new_f_allocation *
		sizeof(double complex *));
	if (clfpp == NULL) {
	    return -1;
	}
	(void)memset((void *)&clfpp[old_f_allocation], 0,
		(new_f_allocation - old_f_allocation) *
		sizeof(double complex *));
	vdp->vd_data = clfpp;

	/*
	 * Add the new sub-vectors.
	 */
	for (int findex = old_f_allocation; findex < new_f_allocation;
		++findex) {
	    if ((vdip->vdi_flags & VF_PER_F_Z0) &&
		    vdip->vdi_p_allocation != 0) {
		if ((vdip->vdi_z0_vector_vector[findex] =
			    calloc(vdip->vdi_p_allocation,
				sizeof(double complex))) == NULL) {
		    return -1;
		}
		for (int i = 0; i < vdip->vdi_p_allocation; ++i) {
		    vdip->vdi_z0_vector_vector[findex][i] = VNADATA_DEFAULT_Z0;
		}
	    }
	    if (vdip->vdi_m_allocation != 0) {
		if ((vdp->vd_data[findex] = calloc(vdip->vdi_m_allocation,
				sizeof(double complex))) == NULL) {
		    if (vdip->vdi_flags & VF_PER_F_Z0) {
			free((void *)vdip->vdi_z0_vector_vector[findex]);
			vdip->vdi_z0_vector_vector[findex] = NULL;
		    }
		    return -1;
		}
	    }
	    ++vdip->vdi_f_allocation;
	}
	assert(vdip->vdi_f_allocation == new_f_allocation);
    }
    return 0;
}

/*
 * vnadata_alloc: allocate an empty vnadata_t object
 */
vnadata_t *vnadata_alloc()
{
    vnadata_internal_t *vdip;

    if ((vdip = malloc(sizeof(vnadata_internal_t))) == NULL) {
	return NULL;
    }
    (void)memset((void *)vdip, 0, sizeof(vnadata_internal_t));
    vdip->vdi_magic = VDI_MAGIC;

    return &vdip->vdi_vd;
}

/*
 * vnadata_resize: redefine the dimensions and type
 *   @vdp: object pointer
 *   @frequencies: new number of frequencies
 *   @rows: new number of rows
 *   @columns: new number of columns
 *   @type: new parameter type
 *
 * Note:
 *   Increasing the number of frequencies or the number of rows is
 *   value preserving; however, we make no effort to reorganize the
 *   data if you increase the number of columns.
 *
 * Invariant:
 *   Cells beyond the current frequencies, cells or ports values
 *   are always filled with initial values.
 */
int vnadata_resize(vnadata_t *vdp, int frequencies, int rows, int columns,
	vnadata_parameter_type_t type)
{
    vnadata_internal_t *vdip;
    int old_ports, new_ports;
    int old_cells, new_cells;

    /*
     * Check parameters
     */
    if (vdp == NULL || frequencies < 0 || rows < 0 || columns < 0) {
	errno = EINVAL;
	return -1;
    }
    vdip = VDP_TO_VDIP(vdp);
    if (vdip->vdi_magic != VDI_MAGIC) {
	errno = EINVAL;
	return -1;
    }
    if (_vnadata_validate(rows, columns, type) == -1) {
	return -1;
    }
    old_ports = MAX(vdp->vd_rows, vdp->vd_columns);
    new_ports = MAX(rows, columns);
    old_cells = vdp->vd_rows * vdp->vd_columns;
    new_cells = rows * columns;

    /*
     * Widen the (inner) z0 vector(s) within old_f_allocation as needed.
     */
    if (_vnadata_extend_p(vdip, new_ports) == -1) {
	return -1;
    }

    /*
     * Extend the matrix allocation within old_f_allocation as needed.
     */
    if (_vnadata_extend_m(vdip, new_cells) == -1) {
	return -1;
    }

    /*
     * Extend the frequency allocation as needed.
     */
    if (_vnadata_extend_f(vdip, frequencies) == -1) {
	return -1;
    }

    /*
     * Re-initialize vacated inner Z0 vector cells.
     */
    if (new_ports < old_ports) {
	if (vdip->vdi_flags & VF_PER_F_Z0) {
	    for (int findex = 0; findex < vdp->vd_frequencies; ++findex) {
		for (int i = new_ports; i < old_ports; ++i) {
		    vdip->vdi_z0_vector_vector[findex][i] = VNADATA_DEFAULT_Z0;
		}
	    }
	} else {
	    for (int i = new_ports; i < old_ports; ++i) {
		vdip->vdi_z0_vector[i] = VNADATA_DEFAULT_Z0;
	    }
	}
    }

    /*
     * Zero vacated matrix cells.
     */
    if (new_cells < old_cells) {
	for (int findex = 0; findex < vdp->vd_frequencies; ++findex) {
	    (void)memset((void *)&vdp->vd_data[findex][new_cells], 0,
		(old_cells - new_cells) * sizeof(double complex));
	}
    }

    /*
     * Re-initialize vacated frequency rows.
     */
    if (frequencies < vdp->vd_frequencies) {
	(void)memset((void *)&vdp->vd_frequency_vector[frequencies], 0,
		(vdp->vd_frequencies - frequencies) * sizeof(double));
	if (vdip->vdi_flags & VF_PER_F_Z0) {
	    for (int findex = vdp->vd_frequencies - 1; findex >= frequencies;
		    --findex) {
		for (int i = 0; i < old_ports; ++i) {
		    vdip->vdi_z0_vector_vector[findex][i] = VNADATA_DEFAULT_Z0;
		}
	    }
	}
	for (int findex = vdp->vd_frequencies - 1; findex >= frequencies;
		--findex) {
	    (void)memset((void *)vdp->vd_data[findex], 0,
		    old_cells * sizeof(double complex));
	}
    }

    /*
     * Set the new type and dimensions.
     */
    vdp->vd_type	= type;
    vdp->vd_frequencies = frequencies;
    vdp->vd_rows	= rows;
    vdp->vd_columns	= columns;

    return 0;
}

/*
 * vnadata_init: resize and initialize a vnadata_t object
 *   @frequencies: number of frequency points
 *   @rows: number of matrix rows
 *   @columns: number of matrix columns
 *   @type: matrix type (see above)
 */
int vnadata_init(vnadata_t *vdp, int frequencies, int rows,
	int columns, vnadata_parameter_type_t type)
{
    //ZZ: Keep for now, since this will test more code paths.  But this
    // would be more robust if it manually initialized everything up to
    // the existing allocations, then called vnadata_resize for additional
    // allocation.
    (void)vnadata_resize(vdp, 0, 0, 0, VPT_UNDEF);
    (void)vnadata_set_all_z0(vdp, VNADATA_DEFAULT_Z0);
    return vnadata_resize(vdp, frequencies, rows, columns, type);
}

/*
 * vnadata_free: free a vnadata_t object
 *   @vdp: object to free
 */
void vnadata_free(vnadata_t *vdp)
{
    if (vdp != NULL) {
	vnadata_internal_t *vdip = VDP_TO_VDIP(vdp);

	assert(vdip->vdi_magic == VDI_MAGIC);
	if (vdip->vdi_flags & VF_PER_F_Z0) {
	    for (int findex = 0; findex < vdip->vdi_f_allocation; ++findex) {
		free((void *)vdip->vdi_z0_vector_vector[findex]);
	    }
	    free((void *)vdip->vdi_z0_vector_vector);
	} else {
	    free((void *)vdip->vdi_z0_vector);
	}
	free((void *)vdp->vd_frequency_vector);
	if (vdp->vd_data != NULL) {
	    for (int findex = 0; findex < vdip->vdi_f_allocation; ++findex) {
		free((void *)vdp->vd_data[findex]);
	    }
	    free((void *)vdp->vd_data);
	}
	free((void *)vdip);
    }
}
