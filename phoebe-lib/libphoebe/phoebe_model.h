#ifndef PHOEBE_MODEL_H
	#define PHOEBE_MODEL_H 1

typedef int PHOEBE_star_id;

typedef struct PHOEBE_star_surface {
	int     elemno;
	double *theta;
	double *phi;
	double *rho;
	double *grad;
	/* These are WD-compatible arrays and may be removed in future: */
	int    *mmsave;
	double *sinth;
	double *costh;
	double *sinphi;
	double *cosphi;
} PHOEBE_star_surface;

PHOEBE_star_surface *phoebe_star_surface_new           ();
PHOEBE_star_surface *phoebe_star_surface_rasterize     (int gridsize);
int                  phoebe_star_surface_compute_radii (PHOEBE_star_surface *surface, double Omega, double q, double D, double F);
int                  phoebe_star_surface_compute_grads (PHOEBE_star_surface *surface, double q, double D, double F);
int                  phoebe_star_surface_free          (PHOEBE_star_surface *surface);

typedef struct PHOEBE_star {
	PHOEBE_star_id       id;
	PHOEBE_star_surface *surface;
} PHOEBE_star;

/* Roche model computation: */

double phoebe_compute_polar_radius (double Omega, double D, double q);
double phoebe_compute_radius       (double rp, double q, double D, double F, double lambda, double nu);
double phoebe_compute_gradient     (double r,  double q, double D, double F, double lambda, double nu);

#endif
