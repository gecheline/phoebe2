#include <stdlib.h>

#include <phoebe/phoebe.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>

#include "phoebe_gui_build_config.h"

#include "phoebe_gui_accessories.h"
#include "phoebe_gui_callbacks.h"
#include "phoebe_gui_error_handling.h"
#include "phoebe_gui_plotting.h"
#include "phoebe_gui_treeviews.h"
#include "phoebe_gui_types.h"

#ifdef __MINGW32__
#include <glib/gfileutils.h>
#include <windows.h>
#include <winuser.h>
#else
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#define gui_plot_width(x) (x->width - 2 * x->layout->xmargin - x->leftmargin - x->layout->rmargin)
#define gui_plot_height(x) (x->height - 2 * x->layout->ymargin - x->layout->tmargin - x->layout->bmargin)

static const double dash_coarse_grid[] = { 4.0, 1.0 };
static const double dash_fine_grid[] = { 1.0 };

GUI_plot_layout *gui_plot_layout_new ()
{
	GUI_plot_layout *layout = phoebe_malloc (sizeof (*layout));

	layout->lmargin = 10;
	layout->rmargin = 10;
	layout->tmargin = 10;
	layout->bmargin = 30;

	layout->xmargin = 10;
	layout->ymargin = 10;

	layout->label_lmargin = 2;
	layout->label_rmargin = 3;

	layout->x_tick_length = layout->xmargin - 2;
	layout->y_tick_length = layout->ymargin - 2;

	return layout;
}

int gui_plot_layout_free (GUI_plot_layout *layout)
{
	free (layout);

	return SUCCESS;
}

GUI_plot_data *gui_plot_data_new ()
{
	/*
	 * Allocates memory for the plot data structure and initializes its fields.
	 */

	GUI_plot_data *data = phoebe_malloc (sizeof (*data));

	data->layout      = gui_plot_layout_new ();
	data->canvas      = NULL;
	data->request     = NULL;
	data->objno       = 0;
	data->x_offset    = 0.0;
	data->y_offset    = 0.0;
	data->zoom_level  = 0;
	data->zoom        = 0.0;
	data->leftmargin  = data->layout->lmargin;
	data->y_min       = 0.0;
	data->y_max       = 1.0;
	data->y_autoscale = TRUE;

	return data;
}

int gui_plot_data_free (GUI_plot_data *data)
{
#warning IMPLEMENT gui_plot_data_free FUNCTION
}

void gui_plot_offset_zoom_limits (double min_value, double max_value, double offset, double zoom, double *newmin, double *newmax)
{
	*newmin = min_value + offset * (max_value - min_value) - zoom * (max_value - min_value);
	*newmax = max_value + offset * (max_value - min_value) + zoom * (max_value - min_value);
	//printf ("min = %lf, max = %lf, offset = %lf, zoom = %lf, newmin = %lf, newmax = %lf\n", min_value, max_value, offset, zoom, *newmin, *newmax);
}

bool gui_plot_xvalue (GUI_plot_data *data, double value, double *x)
{
	double xmin, xmax;
	gui_plot_offset_zoom_limits (data->x_ll, data->x_ul, data->x_offset, data->zoom, &xmin, &xmax);
	*x = data->leftmargin + data->layout->xmargin + (value - xmin) * gui_plot_width(data)/(xmax - xmin);
	if (*x < data->leftmargin) return FALSE;
	if (*x > data->width - data->layout->rmargin) return FALSE;
	return TRUE;
}

bool gui_plot_yvalue (GUI_plot_data *data, double value, double *y)
{
	double ymin, ymax;

	if (data->y_autoscale)
		gui_plot_offset_zoom_limits (data->y_min, data->y_max, data->y_offset, data->zoom, &ymin, &ymax);
	else
		gui_plot_offset_zoom_limits (data->y_ll, data->y_ul, data->y_offset, data->zoom, &ymin, &ymax);
	
	*y = data->height - (data->layout->bmargin + data->layout->ymargin) - (value - ymin) * gui_plot_height(data)/(ymax - ymin);
	if (*y < data->layout->tmargin) return FALSE;
	if (*y > data->height - data->layout->bmargin) return FALSE;
	return TRUE;
}

void gui_plot_coordinates_from_pixels (GUI_plot_data *data, double xpix, double ypix, double *xval, double *yval)
{
	double xmin, xmax, ymin, ymax;

	gui_plot_offset_zoom_limits (data->x_ll, data->x_ul, data->x_offset, data->zoom, &xmin, &xmax);

	if (data->y_autoscale)
		gui_plot_offset_zoom_limits (data->y_min, data->y_max, data->y_offset, data->zoom, &ymin, &ymax);
	else
		gui_plot_offset_zoom_limits (data->y_ll, data->y_ul, data->y_offset, data->zoom, &ymin, &ymax);
	
	*xval = xmin + (xmax - xmin) * (xpix - (data->leftmargin + data->layout->xmargin))/gui_plot_width(data);
	*yval = ymax - (ymax - ymin) * (ypix - (data->layout->tmargin+data->layout->ymargin))/gui_plot_height(data);
}

bool gui_plot_tick_values (double low_value, double high_value, double *first_tick, double *tick_spacing, int *ticks, int *minorticks, char format[])
{
	int logspacing, factor;
	double spacing;

	logspacing = floor(log10(high_value-low_value)) - 1;
	factor = ceil((high_value-low_value)/pow(10, logspacing + 1));
//	printf ("low = %lf, hi = %lf, factor = %d, logspacing = %d\n", low_value, high_value, factor, logspacing);

	if (factor > 5) {
		logspacing++;
		factor = 1;
		*minorticks = 2;
	}
	else {
		if ((factor == 3) || (factor == 4)) {
			factor = 5;
		}
		*minorticks = factor;
	}
	spacing = factor * pow(10, logspacing);

	*tick_spacing = spacing;
	*first_tick = floor(low_value/spacing) * spacing;
	*ticks = ceil((high_value-low_value)/spacing) + 2;
	sprintf(format, "%%.%df", (logspacing > 0) ? 0 : -logspacing);
//	printf("ticks = %d, format = %s, first_tick = %lf, spacing = %lf, factor = %d, logspacing = %d\n", *ticks, format, *first_tick, spacing, factor, logspacing);
	return TRUE;
}

void gui_plot_clear_canvas (GUI_plot_data *data)
{
	GtkWidget *widget = data->container;

	PHOEBE_column_type dtype;
	int ticks, minorticks;
	double first_tick, tick_spacing;
	double x, ymin, ymax;
	char format[20], label[20];
	cairo_text_extents_t te;

	if (data->canvas)
		cairo_destroy (data->canvas);

	data->canvas = gdk_cairo_create (widget->window);
	data->width  = widget->allocation.width;
	data->height = widget->allocation.height;

	cairo_set_source_rgb (data->canvas, 0, 0, 0);
	cairo_set_line_width (data->canvas, 1);

	/* Determine the lowest and highest y value that will be plotted: */
	data->leftmargin = data->layout->lmargin;
	phoebe_column_get_type (&dtype, data->y_request);
	gui_plot_coordinates_from_pixels (data, 0, data->layout->tmargin, &x, &ymax);
	gui_plot_coordinates_from_pixels (data, 0, data->height - data->layout->bmargin, &x, &ymin);
	gui_plot_tick_values ( ((dtype == PHOEBE_COLUMN_MAGNITUDE) ? ymax : ymin), ((dtype == PHOEBE_COLUMN_MAGNITUDE) ? ymin : ymax), &first_tick, &tick_spacing, &ticks, &minorticks, format);

	// Calculate how large the y labels will be
	cairo_select_font_face (data->canvas, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size (data->canvas, 12);

	sprintf(label, format, ymin);
	cairo_text_extents (data->canvas, label, &te);
	if (te.width + te.x_bearing + data->layout->label_lmargin + data->layout->label_rmargin > data->leftmargin) data->leftmargin = te.width + te.x_bearing + data->layout->label_lmargin + data->layout->label_rmargin;
	sprintf(label, format, ymax);
	cairo_text_extents (data->canvas, label, &te);
	if (te.width + te.x_bearing + data->layout->label_lmargin + data->layout->label_rmargin > data->leftmargin) data->leftmargin = te.width + te.x_bearing + data->layout->label_lmargin + data->layout->label_rmargin;

	cairo_rectangle (data->canvas, data->leftmargin, data->layout->tmargin, data->width - data->leftmargin - data->layout->rmargin, data->height - data->layout->tmargin - data->layout->bmargin);
	cairo_stroke (data->canvas);

	return;
}

gboolean on_plot_area_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
	/*
	 * on_plot_area_expose_event:
	 * @widget: plot container
	 * @event: expose event
	 * @user_data: #GUI_plot_data structure
	 *
	 * This callback is invoked every time the plot needs to be redrawn. For
	 * example, it is called on resizing, detaching, obscuring, etc. It is
	 * thus the only function that is allowed to call the actual drawing
	 * function. It is also responsible for plotting the graph box.
	 *
	 * Returns: #FALSE to keep the propagation of the signal.
	 */

	GUI_plot_data *data = (GUI_plot_data *) user_data;

	gui_plot_clear_canvas (data);
	gui_plot_area_draw (data, NULL);

	return FALSE;
}

gboolean on_plot_area_enter (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data)
{
	gtk_widget_grab_focus (widget);
	return TRUE;
}

int gui_plot_get_closest (GUI_plot_data *data, double x, double y, int *cp, int *ci)
{
	/*
	 * gui_plot_get_closest:
	 * @x:  pointer x coordinate
	 * @y:  pointer y coordinate
	 * @cp: closest passband index placeholder
	 * @ci: closest data index placeholder
	 *
	 * Returns the passband index @cp and the data index @ci of the closest
	 * point to the passed (@x,@y) coordinates. This function is *HIGHLY*
	 * unoptimized and can (and should) perform several times better.
	 */

	int i, p;
	double cx, cy, dx, dy, cf, cf0;

	*cp = 0;
	*ci = 0;

	for (p = 0; p < data->objno; p++)
		if (data->request[p].query)
			break;
	if (p == data->objno)
		return GUI_ERROR_NO_CURVE_MARKED_FOR_PLOTTING;

	*cp = p;

	/* Starting with the first data point in the first passband: */
	cx = data->request[p].query->indep->val[0];
	cy = data->request[p].query->dep->val[0] + data->request[p].offset;
	dx = data->x_max-data->x_min;
	dy = data->y_max-data->y_min;
	cf0 = (x-cx)*(x-cx)/dx/dx+(y-cy)*(y-cy)/dy/dy;

	for ( ; p < data->objno; p++) {
		if (!data->request[p].query) continue;
		for (i = 0; i < data->request[p].query->indep->dim; i++) {
			cx = data->request[p].query->indep->val[i];
			cy = data->request[p].query->dep->val[i] + data->request[p].offset;

			cf = (x-cx)*(x-cx)/dx/dx+(y-cy)*(y-cy)/dy/dy;
			if (cf < cf0) {
				*cp = p;
				*ci = i;
				cf0 = cf;
			}
		}
	}

	return SUCCESS;
}

gboolean on_plot_area_motion (GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
	GUI_plot_data *data = (GUI_plot_data *) user_data;

	double x, y;
	char x_str[20], y_str[20];
	char cp_str[255], *cp_ptr, cx_str[20], cy_str[20];

	int cp, ci, p;

	gui_plot_coordinates_from_pixels (data, event->x, event->y, &x, &y);

	sprintf (x_str, "%lf", x);
	sprintf (y_str, "%lf", y);

	gtk_label_set_text (GTK_LABEL (data->x_widget), x_str);
	gtk_label_set_text (GTK_LABEL (data->y_widget), y_str);

	if (gui_plot_get_closest (data, x, y, &cp, &ci) != SUCCESS)
		return FALSE;

	for (p = 0; p < data->objno; p++)
		if (data->request[p].query)
			break;

	if (p == data->objno)
		return FALSE;

	switch (data->ptype) {
		case GUI_PLOT_LC:
			phoebe_parameter_get_value (phoebe_parameter_lookup ("phoebe_lc_id"), cp, &cp_ptr);
		break;
		case GUI_PLOT_RV:
#warning CURVE_ID_RECOGNITION_FAILS_FOR_RV_CURVES
			phoebe_parameter_get_value (phoebe_parameter_lookup ("phoebe_rv_id"), cp, &cp_ptr);
		break;
		case GUI_PLOT_MESH:
			/* Fall through */
		break;
		default:
			gui_error ("Exception handler invoked", "PHOEBE ran into an unhandled condition in on_plot_area_motion(), please report this!");
			return FALSE;
	}

	sprintf (cp_str, "in %s:", cp_ptr);

	sprintf (cx_str, "%lf", data->request[cp].query->indep->val[ci]);
	sprintf (cy_str, "%lf", data->request[cp].query->dep->val[ci]);

	gtk_label_set_text (GTK_LABEL (data->cp_widget), cp_str);
	gtk_label_set_text (GTK_LABEL (data->cx_widget), cx_str);
	gtk_label_set_text (GTK_LABEL (data->cy_widget), cy_str);

	return FALSE;
}

void on_plot_button_clicked (GtkButton *button, gpointer user_data)
{
	/*
	 * on_plot_button_clicked:
	 * @button: Plot button widget 
	 * @user_data: #GUI_plot_data structure
	 *
	 * This is the main allocation plotting function. It queries all widgets
	 * for their content and stores them to the #GUI_plot_data structure for
	 * follow-up plotting. The plotting itself is *not* done by this function;
	 * rather, the call to a graph refresh is issued that, in turn, invokes
	 * the expose event handler that governs the plotting. The purpose of
	 * this function is to get everything ready for the expose event handler.
	 */

	GUI_plot_data *data = (GUI_plot_data *) user_data;
	int i, j, status;
	PHOEBE_column_type itype, dtype;
	double x_min, x_max, y_min, y_max;

	bool plot_obs, plot_syn, first_time = YES;

	phoebe_gui_debug ("* entering on_plot_button_clicked().\n");

	/* Read in parameter values from their respective widgets: */
	gui_get_values_from_widgets ();

	if (data->ptype == GUI_PLOT_LC || data->ptype == GUI_PLOT_RV) {
		/* See what is requested: */
		phoebe_column_get_type (&itype, data->x_request);
		phoebe_column_get_type (&dtype, data->y_request);

		for (i = 0; i < data->objno; i++) {
			plot_obs = data->request[i].plot_obs;
			plot_syn = data->request[i].plot_syn;

			/* Free any pre-existing data: */
			if (data->request[i].raw)   phoebe_curve_free (data->request[i].raw);   data->request[i].raw   = NULL;
			if (data->request[i].query) phoebe_curve_free (data->request[i].query); data->request[i].query = NULL;
			if (data->request[i].model) phoebe_curve_free (data->request[i].model); data->request[i].model = NULL;

			/* Prepare observed data (if toggled): */
			if (plot_obs) {
				if (data->ptype == GUI_PLOT_LC)
					data->request[i].raw = phoebe_curve_new_from_pars (PHOEBE_CURVE_LC, i);
				else
					data->request[i].raw = phoebe_curve_new_from_pars (PHOEBE_CURVE_RV, i);
				
				if (!data->request[i].raw) {
					char notice[255];
					plot_obs = NO;
					sprintf (notice, "Observations for curve %d failed to open and cannot be plotted. Please review the information given in the Data tab.", i+1);
					gui_notice ("Observed data not found", notice);
				}

				/* Transform the data to requested types: */
				if (plot_obs) {
					data->request[i].query = phoebe_curve_duplicate (data->request[i].raw);
					switch (data->ptype) {
						case GUI_PLOT_LC:
							status = phoebe_curve_transform (data->request[i].query, itype, dtype, PHOEBE_COLUMN_UNDEFINED);
						break;
						case GUI_PLOT_RV:
							status = phoebe_curve_transform (data->request[i].query, itype, data->request[i].raw->dtype, PHOEBE_COLUMN_UNDEFINED);
						break;
						default:
							gui_error ("Exception handler invoked", "PHOEBE ran into an unhandled condition in on_plot_button_clicked(), please report this!");
							return;
					}
					if (status != SUCCESS) {
						char notice[255];
						plot_obs = NO;
						sprintf (notice, "Observations for curve %d cannot be transformed to the requested plotting axes. Plotting will be suppressed.", i+1);
						gui_notice ("Observed data transformation failure", notice);
					}
				}

				/* Alias data if requested: */
				if (data->alias && plot_obs) {
					status = phoebe_curve_alias (data->request[i].query, data->x_ll, data->x_ul);
					if (status != SUCCESS) {
						char notice[255];
						plot_obs = NO;
						sprintf (notice, "Observations for curve %d cannot be aliased. Plotting will be suppressed.", i+1);
						gui_notice ("Observed data aliasing failure", notice);
					}
				}

				/* Calculate residuals when requested */
				if (data->residuals && plot_obs) {
					PHOEBE_vector *indep = phoebe_vector_duplicate (data->request[i].query->indep);
					data->request[i].model = phoebe_curve_new ();

					data->request[i].model->itype = itype;
					switch (data->ptype) {
						case GUI_PLOT_LC:
							data->request[i].model->type = PHOEBE_CURVE_LC;
							status = phoebe_curve_compute (data->request[i].model, indep, i, itype, dtype);
						break;
						case GUI_PLOT_RV:
						{
							char *param; PHOEBE_column_type rvtype;
							data->request[i].model->type = PHOEBE_CURVE_RV;
							phoebe_parameter_get_value (phoebe_parameter_lookup ("phoebe_rv_dep"), i, &param);
							phoebe_column_get_type (&rvtype, param);
							status = phoebe_curve_compute (data->request[i].model, indep, i, itype, rvtype);
						}
						break;
						default:
							gui_error ("Exception handler invoked", "PHOEBE ran into an unhandled condition in on_plot_button_clicked(), please report this!");
							return;
					}

					if (status != SUCCESS) {
						char notice[255];
						plot_obs = NO;
						sprintf (notice, "Model computation for curve %d failed with the following message: %s", i+1, phoebe_gui_error (status));
						gui_notice ("Model curve computation failed", notice);
					}

					for (j = 0; j < data->request[i].model->indep->dim; j++) {
						data->request[i].query->dep->val[j] -= data->request[i].model->dep->val[j];
						data->request[i].model->dep->val[j] = 0.0;
					}

					if (!plot_syn) {
						phoebe_curve_free (data->request[i].model); 
						data->request[i].model = NULL;
					}
				}

				/* Determine plot limits: */
				if (plot_obs) {
					phoebe_vector_min_max (data->request[i].query->indep, &x_min, &x_max);
					phoebe_vector_min_max (data->request[i].query->dep,   &y_min, &y_max);

					if (first_time) {
						data->x_min = x_min;
						data->x_max = x_max;
						data->y_min = y_min + data->request[i].offset;
						data->y_max = y_max + data->request[i].offset;
						first_time = NO;
					}
					else {
						data->x_min = min (data->x_min, x_min);
						data->x_max = max (data->x_max, x_max);
						data->y_min = min (data->y_min, y_min + data->request[i].offset);
						data->y_max = max (data->y_max, y_max + data->request[i].offset);
					}
				}
			}

			/* Prepare synthetic (model) data (if toggled): */
			if (plot_syn && !(data->residuals)) {
				PHOEBE_vector *indep = phoebe_vector_new_from_range (data->vertices, data->x_ll, data->x_ul);

				data->request[i].model = phoebe_curve_new ();

				switch (data->ptype) {
					case GUI_PLOT_LC:
						data->request[i].model->type = PHOEBE_CURVE_LC;
						status = phoebe_curve_compute (data->request[i].model, indep, i, itype, dtype);
					break;
					case GUI_PLOT_RV:
					{
						char *param; PHOEBE_column_type rvtype;
						data->request[i].model->type = PHOEBE_CURVE_RV;
						phoebe_parameter_get_value (phoebe_parameter_lookup ("phoebe_rv_dep"), i, &param);
						phoebe_column_get_type (&rvtype, param);
						status = phoebe_curve_compute (data->request[i].model, indep, i, itype, rvtype);
					}
					break;
					default:
						gui_error ("Exception handler invoked", "PHOEBE ran into an unhandled condition in on_plot_button_clicked(), please report this!");
						return;
				}
				
				if (status != SUCCESS) {
					char notice[255];
					plot_syn = NO;
					phoebe_curve_free (data->request[i].model);
					data->request[i].model = NULL;
					sprintf (notice, "Model computation for curve %d failed with the following message: %s", i+1, phoebe_gui_error (status));
					gui_notice ("Model curve computation failed", notice);
				}

				gui_fill_sidesheet_res_treeview ();

				/* Plot aliasing for synthetic curves is not implemented by the
				 * library -- phoebe_curve_compute () computes the curve on the
				 * passed range. Perhaps it would be better to have that function
				 * compute *up to* 1.0 in phase and alias the rest. But this is
				 * not so urgent.
				 */

				/* Determine plot limits */
				if (plot_syn) {
					phoebe_vector_min_max (data->request[i].model->indep, &x_min, &x_max);
					phoebe_vector_min_max (data->request[i].model->dep,   &y_min, &y_max);

					if (first_time) {
						data->x_min = x_min;
						data->x_max = x_max;
						data->y_min = y_min + data->request[i].offset;
						data->y_max = y_max + data->request[i].offset;
						first_time = NO;
					}
					else {
						data->x_min = min (data->x_min, x_min);
						data->x_max = max (data->x_max, x_max);
						data->y_min = min (data->y_min, y_min + data->request[i].offset);
						data->y_max = max (data->y_max, y_max + data->request[i].offset);
					}
				}

				phoebe_vector_free (indep);
			}
		}

		/* If we are plotting magnitudes, reverse the y-axis: */
		if (dtype == PHOEBE_COLUMN_MAGNITUDE) {
			double store = data->y_min;
			data->y_min = data->y_max;
			data->y_max = store;
		}
	}
	else /* if (data->ptype == GUI_PLOT_MESH) */ {
		PHOEBE_vector *poscoy, *poscoz;
		char *lcin;
		WD_LCI_parameters *params = phoebe_malloc (sizeof (*params));
		
		status = wd_lci_parameters_get (params, 5, 0);
		if (status != SUCCESS) {
			gui_notice ("Mesh computation failed", "For some mysterious reason (such as a bug in the program) parameter readout failed. Please report this.");
			return;
		}
		
		lcin = phoebe_create_temp_filename ("phoebe_lci_XXXXXX");
		create_lci_file (lcin, params);
		
		poscoy = phoebe_vector_new ();
		poscoz = phoebe_vector_new ();
		status = phoebe_compute_pos_using_wd (poscoy, poscoz, lcin, data->request->phase);
		
		data->request[0].model = phoebe_curve_new ();
		data->request[0].model->indep = poscoy;
		data->request[0].model->dep   = poscoz;

		data->x_ll = -1.1;
		data->x_ul =  1.1;

		data->y_autoscale = FALSE;
		data->y_ll = -1.0; /* Dummies, because the values are later computed  */
		data->y_ul =  1.0; /* so that the aspect=1 in gui_plot_area_draw().   */
		
		phoebe_vector_min_max (poscoy, &x_min, &x_max);
		phoebe_vector_min_max (poscoz, &y_min, &y_max);

		data->x_min = x_min;
		data->x_max = x_max;
		data->y_min = y_min;
		data->y_max = y_max;
	}

	gui_plot_area_refresh (data);

	phoebe_gui_debug ("* leaving on_plot_button_clicked.\n");

	return;
}

int gui_plot_area_refresh (GUI_plot_data *data)
{
	/*
	 * gui_plot_area_refresh:
	 * @data: #GUI_plot_data structure
	 *
	 * This is a small hack to invoke the expose event handler to refresh the
	 * plot. It is not perfectly elegant, but it is simple and is serves the
	 * purpose.
	 *
	 * Returns: #PHOEBE_error_code.
	 */

	gtk_widget_hide (data->container);
	gtk_widget_show (data->container);

	return SUCCESS;
}

void gui_plot_xticks (GUI_plot_data *data)
{
	int i, j, ticks, minorticks;
	double x, first_tick, tick_spacing, value;
	double xmin, xmax, y;
	char format[20];
	char label[20];
	cairo_text_extents_t te;
	
	// Determine the lowest and highest x value that will be plotted
	gui_plot_coordinates_from_pixels (data, data->leftmargin, 0, &xmin, &y);
	gui_plot_coordinates_from_pixels (data, data->width - data->layout->rmargin, 0, &xmax, &y);

	if (!gui_plot_tick_values (xmin, xmax, &first_tick, &tick_spacing, &ticks, &minorticks, format))
		return;

	cairo_set_source_rgb (data->canvas, 0.0, 0.0, 0.0);
	cairo_select_font_face (data->canvas, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size (data->canvas, 12);

	for (i = 0; i < ticks; i++) {
		value = first_tick + i * tick_spacing;
		if (!gui_plot_xvalue (data, value, &x)) continue;

		// Top tick
		cairo_move_to (data->canvas, x, data->layout->tmargin);
		cairo_rel_line_to (data->canvas, 0, data->layout->x_tick_length);
		// Bottom tick
		cairo_move_to (data->canvas, x, data->height - data->layout->bmargin);
		cairo_rel_line_to (data->canvas, 0, - data->layout->x_tick_length);
		cairo_stroke (data->canvas);

		if (data->coarse_grid) {
			cairo_set_line_width (data->canvas, 0.5);
			cairo_set_dash(data->canvas, dash_coarse_grid, sizeof(dash_coarse_grid) / sizeof(dash_coarse_grid[0]), 0);
			cairo_move_to (data->canvas, x, data->layout->tmargin);
			cairo_line_to (data->canvas, x, data->height - data->layout->bmargin);
			cairo_stroke (data->canvas);
		}

		// Print the label
		sprintf(label, format, value);
		cairo_text_extents (data->canvas, label, &te);
		cairo_move_to (data->canvas, x - te.width/2 - te.x_bearing, data->height - te.height + te.y_bearing);
		cairo_show_text (data->canvas, label);
		cairo_stroke (data->canvas);
	}

	// Minor ticks
	for (i = 0; i < ticks - 1; i++) {
		for (j = 1; j < minorticks; j++) {
			if (!gui_plot_xvalue (data, first_tick + (minorticks * i + j) * tick_spacing/minorticks, &x)) continue;

			cairo_move_to (data->canvas, x, data->layout->tmargin);
			cairo_rel_line_to (data->canvas, 0, data->layout->x_tick_length/2);
			cairo_move_to (data->canvas, x, data->height - data->layout->bmargin);
			cairo_rel_line_to (data->canvas, 0, -data->layout->x_tick_length/2);
			cairo_stroke (data->canvas);

			if (data->fine_grid) {
				cairo_set_line_width (data->canvas, 0.5);
				cairo_set_dash(data->canvas, dash_fine_grid, sizeof(dash_fine_grid) / sizeof(dash_fine_grid[0]), 0);
				cairo_move_to (data->canvas, x, data->layout->tmargin);
				cairo_line_to (data->canvas, x, data->height - data->layout->bmargin);
				cairo_stroke (data->canvas);
			}
		}
	}
}

void gui_plot_yticks (GUI_plot_data *data)
{
	int i, j, ticks, minorticks;
	double y, first_tick, tick_spacing, value;
	double ymin, ymax, x;
	char format[20];
	char label[20];
	cairo_text_extents_t te;
	PHOEBE_column_type dtype;
	
	// Determine the lowest and highest y value that will be plotted
	gui_plot_coordinates_from_pixels (data, 0, data->layout->tmargin, &x, &ymax);
	gui_plot_coordinates_from_pixels (data, 0, data->height - data->layout->bmargin, &x, &ymin);

	phoebe_column_get_type (&dtype, data->y_request);
	if (!gui_plot_tick_values ( ((dtype == PHOEBE_COLUMN_MAGNITUDE) ? ymax : ymin), ((dtype == PHOEBE_COLUMN_MAGNITUDE) ? ymin : ymax), &first_tick, &tick_spacing, &ticks, &minorticks, format))
		return;

	cairo_set_source_rgb (data->canvas, 0.0, 0.0, 0.0);
	cairo_select_font_face (data->canvas, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size (data->canvas, 12);

	for (i = 0; i < ticks; i++) {
		value = first_tick + i * tick_spacing;
		if (!gui_plot_yvalue (data, value, &y)) continue;

		// Left tick
		cairo_move_to (data->canvas, data->leftmargin, y);
		cairo_rel_line_to (data->canvas, data->layout->y_tick_length, 0);
		// Right tick
		cairo_move_to (data->canvas, data->width - data->layout->rmargin, y);
		cairo_rel_line_to (data->canvas, - data->layout->y_tick_length, 0);
		cairo_stroke (data->canvas);

		if (data->coarse_grid) {
			cairo_set_line_width (data->canvas, 0.5);
			cairo_set_dash(data->canvas, dash_coarse_grid, sizeof(dash_coarse_grid) / sizeof(dash_coarse_grid[0]), 0);
			cairo_move_to (data->canvas, data->leftmargin, y);
			cairo_line_to (data->canvas, data->width - data->layout->rmargin, y);
			cairo_stroke (data->canvas);
		}

		// Print the label
		sprintf(label, format, value);
		cairo_text_extents (data->canvas, label, &te);
		cairo_move_to (data->canvas, data->leftmargin - te.width - te.x_bearing - data->layout->label_rmargin, y - te.height/2 - te.y_bearing);
		cairo_show_text (data->canvas, label);
	}
	// Minor ticks
	for (i = 0; i < ticks - 1; i++) {
		for (j = 1; j < minorticks; j++) {
			if (!gui_plot_yvalue (data, first_tick + (minorticks * i + j) * tick_spacing/minorticks, &y)) continue;
			cairo_move_to (data->canvas, data->leftmargin, y);
			cairo_rel_line_to (data->canvas, data->layout->y_tick_length/2, 0);
			cairo_move_to (data->canvas, data->width - data->layout->rmargin, y);
			cairo_rel_line_to (data->canvas, -data->layout->y_tick_length/2, 0);
			cairo_stroke (data->canvas);

			if (data->fine_grid) {
				cairo_set_line_width (data->canvas, 0.5);
				cairo_set_dash(data->canvas, dash_fine_grid, sizeof(dash_fine_grid) / sizeof(dash_fine_grid[0]), 0);
				cairo_move_to (data->canvas, data->leftmargin, y);
				cairo_line_to (data->canvas, data->width - data->layout->rmargin, y);
				cairo_stroke (data->canvas);
			}
		}
	}
	cairo_stroke (data->canvas);
}

void gui_plot_interpolate_to_border (GUI_plot_data *data, double xin, double yin, double xout, double yout, double *xborder, double *yborder)
{
	bool x_outside_border = FALSE, y_outside_border = FALSE;
	double xmargin ,ymargin;

	if (xout < data->leftmargin) {
		xmargin = data->leftmargin;
		x_outside_border = TRUE;
	}
	else if (xout > data->width - data->layout->rmargin) {
		xmargin = data->width - data->layout->rmargin;
		x_outside_border = TRUE;
	}

	if (yout < data->layout->tmargin) {
		ymargin = data->layout->tmargin;
		y_outside_border = TRUE;
	}
	else if (yout > data->height - data->layout->bmargin) {
		ymargin = data->height - data->layout->bmargin;
		y_outside_border = TRUE;
	}

	if (x_outside_border && y_outside_border) {
		/* Both xout and yout lie outside the border */
		*yborder = yout + (yin - yout) * (xmargin - xout)/(xin - xout);
		if ((*yborder < data->layout->tmargin) && (*yborder > data->height - data->layout->bmargin)) {
			*yborder = ymargin;
			*xborder = xout + (xin - xout) * (ymargin - yout)/(yin - yout);
		}
		else {
			*xborder = xmargin;
		}
	}
	else if (x_outside_border) {
		/* Only xout lies outside the border */
		*xborder = xmargin;
		*yborder = yout + (yin - yout) * (xmargin - xout)/(xin - xout);
	}
	else {
		/* Only yout lies outside the border */
		*yborder = ymargin;
		*xborder = xout + (xin - xout) * (ymargin - yout)/(yin - yout);
	}
}

int gui_plot_area_draw (GUI_plot_data *data, FILE *redirect)
{
	int i, j;
	double x, y, aspect;
	bool needs_ticks = FALSE;

	if (data->ptype == GUI_PLOT_MESH && data->request[0].model) {
		if (!redirect)
			cairo_set_source_rgb (data->canvas, 0, 0, 1);
		else
			fprintf (redirect, "# Mesh plot -- plane of sky (v, w) coordinates at phase %lf:\n", data->request[0].phase);
		
		aspect = gui_plot_height (data)/gui_plot_width (data);
		data->y_ll = aspect*data->x_ll;
		data->y_ul = aspect*data->x_ul;
		
		for (j = 0; j < data->request[0].model->indep->dim; j++) {
			if (!gui_plot_xvalue (data, data->request[0].model->indep->val[j], &x)) continue;
			if (!gui_plot_yvalue (data, data->request[0].model->dep->val[j], &y)) continue;
			
			if (!redirect) {
				cairo_arc (data->canvas, x, y, 2.0, 0, 2*M_PI);
				cairo_stroke (data->canvas);
			}
			else
				fprintf (redirect, "% lf\t% lf\n", data->request[0].model->indep->val[j], data->request[0].model->dep->val[j]);
		}

		needs_ticks = TRUE;
	}

	if (data->ptype == GUI_PLOT_LC || data->ptype == GUI_PLOT_RV) {
		for (i = 0; i < data->objno; i++) {
			if (data->request[i].query) {
				if (!redirect)
					cairo_set_source_rgb (data->canvas, 0, 0, 1);
				else
					fprintf (redirect, "# Observed data-set %d:\n", i);

				for (j = 0; j < data->request[i].query->indep->dim; j++) {
					if (!gui_plot_xvalue (data, data->request[i].query->indep->val[j], &x)) continue;
					if (!gui_plot_yvalue (data, data->request[i].query->dep->val[j] + data->request[i].offset, &y)) continue;
					if (data->request[i].query->flag->val.iarray[j] == PHOEBE_DATA_OMITTED) continue;

					if (!redirect) {
						cairo_arc (data->canvas, x, y, 2.0, 0, 2*M_PI);
						cairo_stroke (data->canvas);
					}
					else
						fprintf (redirect, "%lf\t%lf\n", data->request[i].query->indep->val[j], data->request[i].query->dep->val[j] + data->request[i].offset);
				}
				needs_ticks = TRUE;
			}

			if (data->request[i].model) {
				bool last_point_plotted = FALSE;
				bool x_in_plot, y_in_plot;
				double lastx, lasty;

				if (redirect)
					fprintf (redirect, "# Synthetic data set %d:\n", i);
				else
					cairo_set_source_rgb (data->canvas, 1, 0, 0);

				for (j = 0; j < data->request[i].model->indep->dim; j++) {
					x_in_plot = gui_plot_xvalue (data, data->request[i].model->indep->val[j], &x);
					y_in_plot = gui_plot_yvalue (data, data->request[i].model->dep->val[j] + data->request[i].offset, &y);
					if (!x_in_plot || !y_in_plot) {
						if (last_point_plotted) {
							double xborder, yborder;
							gui_plot_interpolate_to_border (data, lastx, lasty, x, y, &xborder, &yborder);
							if (!redirect)
								cairo_line_to (data->canvas, xborder, yborder);
							else
								fprintf (redirect, "%lf\t%lf\n", data->request[i].model->indep->val[j], data->request[i].model->dep->val[j] + data->request[i].offset);

							last_point_plotted = FALSE;
						}
					}
					else {
						if (!last_point_plotted && (j > 0)) {
							double xborder, yborder;
							gui_plot_interpolate_to_border (data, x, y, lastx, lasty, &xborder, &yborder);
							if (!redirect)
								cairo_move_to (data->canvas, xborder, yborder);
							last_point_plotted = TRUE;
						}
						if (last_point_plotted) {
							if (!redirect)
								cairo_line_to (data->canvas, x, y);
							else
								fprintf (redirect, "%lf\t%lf\n", data->request[i].model->indep->val[j], data->request[i].model->dep->val[j] + data->request[i].offset);
						}
						else {
							if (!redirect)
								cairo_move_to (data->canvas, x, y);
							last_point_plotted = TRUE;
						}
					}
					lastx = x;
					lasty = y;
				}

				if (!redirect)
					cairo_stroke (data->canvas);

				needs_ticks = TRUE;
			}
		}
	}

	if (needs_ticks && !redirect) {
		gui_plot_xticks (data);
		gui_plot_yticks (data);
	}

	return SUCCESS;
}

void on_lc_plot_treeview_row_changed (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	GUI_plot_data *data = (GUI_plot_data *) user_data;
	int i, rows;
	GtkTreeIter traverser;

	bool obs, syn;
	char *obscolor, *syncolor;
	double offset;

	/* Count rows in the model: */
	rows = gtk_tree_model_iter_n_children (tree_model, NULL);
	printf ("no. of rows: %d\n", rows);

	/* Reallocate memory for the plot properties: */
	data->request = phoebe_realloc (data->request, rows * sizeof (*(data->request)));
	if (rows == 0) data->request = NULL;

	/* Traverse all rows and update the values in the plot structure: */
	for (i = 0; i < rows; i++) {
		gtk_tree_model_iter_nth_child (tree_model, &traverser, NULL, i);
		gtk_tree_model_get (tree_model, &traverser, LC_COL_PLOT_OBS, &obs, LC_COL_PLOT_SYN, &syn, LC_COL_PLOT_OBS_COLOR, &obscolor, LC_COL_PLOT_SYN_COLOR, &syncolor, LC_COL_PLOT_OFFSET, &offset, -1);
		data->request[i].plot_obs = obs;
		data->request[i].plot_syn = syn;
		data->request[i].obscolor = obscolor;
		data->request[i].syncolor = syncolor;
		data->request[i].offset   = offset;
		data->request[i].raw      = NULL;
		data->request[i].query    = NULL;
		data->request[i].model    = NULL;
printf ("row %d/%d: (%d, %d, %s, %s, %lf)\n", i, rows, data->request[i].plot_obs, data->request[i].plot_syn, data->request[i].obscolor, data->request[i].syncolor, data->request[i].offset);
	}
	data->objno = rows;

	return;
}

void on_rv_plot_treeview_row_changed (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	GUI_plot_data *data = (GUI_plot_data *) user_data;
	int i, rows;
	GtkTreeIter traverser;

	bool obs, syn;
	char *obscolor, *syncolor;
	double offset;

	printf ("* entered on_rv_plot_treeview_row_changed() function.\n");

	/* Count rows in the model: */
	rows = gtk_tree_model_iter_n_children (tree_model, NULL);
	printf ("no. of rows: %d\n", rows);

	/* Reallocate memory for the plot properties: */
	data->request = phoebe_realloc (data->request, rows * sizeof (*(data->request)));
	if (rows == 0) data->request = NULL;

	/* Traverse all rows and update the values in the plot structure: */
	for (i = 0; i < rows; i++) {
		gtk_tree_model_iter_nth_child (tree_model, &traverser, NULL, i);
		gtk_tree_model_get (tree_model, &traverser, RV_COL_PLOT_OBS, &obs, RV_COL_PLOT_SYN, &syn, RV_COL_PLOT_OBS_COLOR, &obscolor, RV_COL_PLOT_SYN_COLOR, &syncolor, RV_COL_PLOT_OFFSET, &offset, -1);
		data->request[i].plot_obs = obs;
		data->request[i].plot_syn = syn;
		data->request[i].obscolor = obscolor;
		data->request[i].syncolor = syncolor;
		data->request[i].offset   = offset;
		data->request[i].raw      = NULL;
		data->request[i].query    = NULL;
		data->request[i].model    = NULL;
printf ("row %d/%d: (%d, %d, %s, %s, %lf)\n", i, rows, data->request[i].plot_obs, data->request[i].plot_syn, data->request[i].obscolor, data->request[i].syncolor, data->request[i].offset);
	}
	data->objno = rows;

	return;
}

void on_plot_treeview_row_deleted (GtkTreeModel *model, GtkTreePath *path, gpointer user_data)
{
	GtkTreeIter iter;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_row_changed (model, path, &iter);

	return;
}

int gui_plot_area_init (GtkWidget *area, GtkWidget *button)
{
	/*
	 * gui_plot_area_init:
	 * @area: plot container widget
	 * @button: "Plot" button
	 * 
	 * This is a generic function to initialize the passed plot area for
	 * plotting with Cairo and connects the data pointer through the plot
	 * button.
	 *
	 * Returns: #PHOEBE_error_code.
	 */

	GtkWidget *widget;
	GUI_plot_data *data;

	/* We initialize the container internally; it's our fault if it fails: */
	if (!area) {
		printf ("*** Please report this crash; include a backtrace and this line:\n");
		printf ("*** NULL pointer passed to the gui_plot_area_init() function.\n");
		exit(0);
	}

	/* Initialize the data structure: */
	data = gui_plot_data_new ();

	/* Get associations from the button and attach change-sensitive callbacks: */
	data->ptype = *((GUI_plot_type *) (g_object_get_data (G_OBJECT (button), "plot_type")));
	data->container = area;

	widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "clear_plot");
	g_signal_connect (widget, "clicked", G_CALLBACK (on_plot_clear_button_clicked), data);

	widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "save_plot");
	g_signal_connect (widget, "clicked", G_CALLBACK (on_plot_save_button_clicked), data);

	gtk_widget_add_events (area, GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK | GDK_ENTER_NOTIFY_MASK);
	g_signal_connect (area, "expose-event", G_CALLBACK (on_plot_area_expose_event), data);

	/* LC/RV parameters: */
	if (data->ptype == GUI_PLOT_LC || data->ptype == GUI_PLOT_RV) {
		g_signal_connect (area, "motion-notify-event", G_CALLBACK (on_plot_area_motion), data);
		g_signal_connect (area, "enter-notify-event", G_CALLBACK (on_plot_area_enter), NULL);
	/*
		g_signal_connect (area, "key-press-event", G_CALLBACK (on_key_press_event), data);
	*/

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "plot_x_request");
		data->x_request = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
		g_signal_connect (widget, "changed", G_CALLBACK (on_combo_box_selection_changed_get_string), &(data->x_request));

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "plot_y_request");
		data->y_request = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
		g_signal_connect (widget, "changed", G_CALLBACK (on_combo_box_selection_changed_get_string), &(data->y_request));

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "phase_start");
		data->x_ll = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));
		g_signal_connect (widget, "value-changed", G_CALLBACK (on_spin_button_value_changed), &(data->x_ll));

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "phase_end");
		data->x_ul = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));
		g_signal_connect (widget, "value-changed", G_CALLBACK (on_spin_button_value_changed), &(data->x_ul));

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "plot_alias_switch");
		data->alias = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
		g_signal_connect (widget, "toggled", G_CALLBACK (on_toggle_button_value_toggled), &(data->alias));

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "plot_resid_switch");
		data->residuals = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
		g_signal_connect (widget, "toggled", G_CALLBACK (on_toggle_button_value_toggled), &(data->residuals));

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "plot_vertices");
		data->vertices = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));
		g_signal_connect (widget, "value-changed", G_CALLBACK (on_spin_button_intvalue_changed), &(data->vertices));

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "coarse_grid_switch");
		data->coarse_grid = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
		g_signal_connect (widget, "toggled", G_CALLBACK (on_toggle_button_value_toggled), &(data->coarse_grid));

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "fine_grid_switch");
		data->fine_grid = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
		g_signal_connect (widget, "toggled", G_CALLBACK (on_toggle_button_value_toggled), &(data->fine_grid));

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "controls_left");
		g_signal_connect (widget, "clicked", G_CALLBACK (on_plot_controls_left_button_clicked), data);

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "controls_down");
		g_signal_connect (widget, "clicked", G_CALLBACK (on_plot_controls_down_button_clicked), data);

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "controls_right");
		g_signal_connect (widget, "clicked", G_CALLBACK (on_plot_controls_right_button_clicked), data);

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "controls_up");
		g_signal_connect (widget, "clicked", G_CALLBACK (on_plot_controls_up_button_clicked), data);

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "controls_reset");
		g_signal_connect (widget, "clicked", G_CALLBACK (on_plot_controls_reset_button_clicked), data);

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "controls_zoomin");
		g_signal_connect (widget, "clicked", G_CALLBACK (on_plot_controls_zoomin_button_clicked), data);

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "controls_zoomout");
		g_signal_connect (widget, "clicked", G_CALLBACK (on_plot_controls_zoomout_button_clicked), data);

		/* Sadly, columns don't have any "changed" signal, so we need to use the
		 * model. That implies different actions for LCs and RVs, so the
		 * implementation is not as elegant as for the rest. Furthermore, removing
		 * rows does not emit the "row-changed" signal, so we have to catch the
		 * "row-deleted" signal as well and re-route it to "row-changed".
		 */

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "plot_passband_info");
		if (data->ptype == GUI_PLOT_LC)
			g_signal_connect (widget, "row-changed", G_CALLBACK (on_lc_plot_treeview_row_changed), data);
		else if (data->ptype == GUI_PLOT_RV)
			g_signal_connect (widget, "row-changed", G_CALLBACK (on_rv_plot_treeview_row_changed), data);

		g_signal_connect (widget, "row-deleted", G_CALLBACK (on_plot_treeview_row_deleted), data);

		/* Assign the widgets (must be GtkLabels) that will keep track of mouse
		 * coordinates:
		 */

		data->x_widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "plot_x_coordinate");
		data->y_widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "plot_y_coordinate");

		data->cp_widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "plot_cp_index");
		data->cx_widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "plot_cx_coordinate");
		data->cy_widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "plot_cy_coordinate");
	}
	else /* if (data->ptype == GUI_PL0T_MESH) */ {
		/* Since there is always a single object, we create the request here. */
		data->objno   = 1;
		data->request = phoebe_malloc (sizeof(*(data->request)));
		data->request[0].plot_obs = FALSE;
		data->request[0].plot_syn = TRUE;
		data->request[0].obscolor = "#000000";
		data->request[0].syncolor = "#000000";
		data->request[0].offset   = 0.0;
		data->request[0].raw      = NULL;
		data->request[0].query    = NULL;
		data->request[0].model    = NULL;

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "controls_zoomin");
		g_signal_connect (widget, "clicked", G_CALLBACK (on_plot_controls_zoomin_button_clicked), data);

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "controls_zoomout");
		g_signal_connect (widget, "clicked", G_CALLBACK (on_plot_controls_zoomout_button_clicked), data);

		widget = (GtkWidget *) g_object_get_data (G_OBJECT (button), "mesh_phase");
		data->request->phase = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));
		g_signal_connect (widget, "value-changed", G_CALLBACK (on_spin_button_value_changed), &(data->request->phase));
	}

	/* Attach a callback that will plot the data: */
	g_signal_connect (button, "clicked", G_CALLBACK (on_plot_button_clicked), data);

	return SUCCESS;
}

int gui_tempfile (char *filename) 
{
#ifdef __MINGW32__
	return g_mkstemp(filename);
#else
	return mkstemp(filename);
#endif
}

int gui_plot_get_curve_limits (PHOEBE_curve *curve, double *xmin, double *ymin, double *xmax, double *ymax)
{
	int i;

	*xmin = 0; *xmax = 0;
	*ymin = 0; *ymax = 0;

	if (!curve)
		return ERROR_CURVE_NOT_INITIALIZED;

	*xmin = curve->indep->val[0]; *xmax = curve->indep->val[0];
	*ymin = curve->dep->val[0];   *ymax = curve->dep->val[0];

	for (i = 1; i < curve->indep->dim; i++) {
		if (curve->flag->val.iarray[i] == PHOEBE_DATA_OMITTED) continue;
		if (*xmin > curve->indep->val[i]) *xmin = curve->indep->val[i];
		if (*xmax < curve->indep->val[i]) *xmax = curve->indep->val[i];
		if (*ymin > curve->dep->val[i]  ) *ymin = curve->dep->val[i];
		if (*ymax < curve->dep->val[i]  ) *ymax = curve->dep->val[i];
	}

	return SUCCESS;
}

int gui_plot_get_offset_zoom_limits (double min, double max, double offset, double zoom, double *newmin, double *newmax)
{
	*newmin = min + offset * (max - min) - (0.1 + zoom) * (max - min);
	*newmax = max + offset * (max - min) + (0.1 + zoom) * (max - min);

	return SUCCESS;
}

int gui_plot_get_plot_limits (PHOEBE_curve *syn, PHOEBE_curve *obs, double *xmin, double *ymin, double *xmax, double *ymax, gboolean plot_syn, gboolean plot_obs, double x_offset, double y_offset, double zoom)
{
	int status;
	double xmin1, xmax1, ymin1, ymax1;              /* Synthetic data limits    */
	double xmin2, xmax2, ymin2, ymax2;              /* Experimental data limits */

	if (plot_syn) {
		status = gui_plot_get_curve_limits (syn, &xmin1, &ymin1, &xmax1, &ymax1);
		if (status != SUCCESS) return status;
	}
	if (plot_obs) {
		status = gui_plot_get_curve_limits (obs, &xmin2, &ymin2, &xmax2, &ymax2);
		if (status != SUCCESS) return status;
	}

	if (plot_syn)
	{
		gui_plot_get_offset_zoom_limits (xmin1, xmax1, x_offset, zoom, xmin, xmax);
		gui_plot_get_offset_zoom_limits (ymin1, ymax1, y_offset, zoom, ymin, ymax);
		xmin1 = *xmin; xmax1 = *xmax; ymin1 = *ymin; ymax1 = *ymax;
	}

	if (plot_obs)
	{
		gui_plot_get_offset_zoom_limits (xmin2, xmax2, x_offset, zoom, xmin, xmax);
		gui_plot_get_offset_zoom_limits (ymin2, ymax2, y_offset, zoom, ymin, ymax);
		xmin2 = *xmin; xmax2 = *xmax; ymin2 = *ymin; ymax2 = *ymax;
	}

	if (plot_syn && plot_obs) {
		if (xmin1 < xmin2) *xmin = xmin1; else *xmin = xmin2;
		if (xmax1 > xmax2) *xmax = xmax1; else *xmax = xmax2;
		if (ymin1 < ymin2) *ymin = ymin1; else *ymin = ymin2;
		if (ymax1 > ymax2) *ymax = ymax1; else *ymax = ymax2;
	}

	return SUCCESS;
}

gint gui_rv_component_index (gint DEP)
{
	/* Returns the index number of the RV file that corresponds to the given component. */

	char *param;
	PHOEBE_column_type dtype;
	int status, index;

	for (index = 0; index <= 1; index++) {
		phoebe_parameter_get_value (phoebe_parameter_lookup ("phoebe_rv_dep"), index, &param);
		status = phoebe_column_get_type (&dtype, param);
		if ((status == SUCCESS) && (dtype == DEP))
			return index;
	}

	return -1;
}

int gui_rv_hjd_minmax (double *hjd_min, double *hjd_max)
{
	gint index[2];
	int i, present = 0;
	
	index[0] = gui_rv_component_index(PHOEBE_COLUMN_PRIMARY_RV);
	index[1] = gui_rv_component_index(PHOEBE_COLUMN_SECONDARY_RV);

	for (i = 0; i <= 1; i++) {
		if (index[i] >= 0) {
			PHOEBE_curve *obs = NULL;
			obs = phoebe_curve_new_from_pars (PHOEBE_CURVE_RV, index[i]);
			if (obs) {
				double min, max;
				phoebe_vector_min_max (obs->indep, &min, &max);
				phoebe_curve_free(obs);
				if (present) {
					if (*hjd_min > min) *hjd_min = min;
					if (*hjd_max < max) *hjd_max = max;
				}
				else {
					present = 1;
					*hjd_min = min;
					*hjd_max = max;
				}
			}
		}
	}

	return present;
}
