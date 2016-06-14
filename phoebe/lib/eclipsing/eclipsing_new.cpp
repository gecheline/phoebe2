/*
  Testing new algorithm for eclipsing.
  
  Profiling:

  g++ -O3 -Wall -std=c++11 eclipsing_new.cpp -o eclipsing_new -pg 
  
  ./eclipsing_new
   
  gprof  eclipsing_new gmon.out > analysis.txt
  
  Reading analysis.txt
   
  Author: Martin Horvat, May 2016
*/ 

#include <iostream>
#include <cmath>
#include <fstream>
#include <limits>

#include "eclipsing.h"
#include "../gen_roche.h"
#include "../triang/triang_marching.h"
#include "../triang/bodies.h"

#include "clipper.h"

#include <ctime>

int main(){
 
 
  clock_t start, end;
     
  //
  // Overcontact case
  //
  
  int  max_triangles = 10000000;
    
  double 
    q = 0.5,
    F = 0.5,
    deltaR = 1,
    Omega0 = 2.65,
    
    delta = 0.01;

  std::vector<double> x_points;
    
  gen_roche::points_on_x_axis(x_points, Omega0, q, F, deltaR);
    
  double  
    x0 = x_points.front(),
    params[5] = {q, F, deltaR, Omega0, x0};   
  
  //
  // make triangulation of the surface
  //
  
  std::cout << "Surface triagulation:";
  
  start = clock();
  
  Tmarching<double, Tgen_roche<double> > march(params);

  std::vector<T3Dpoint<double>> V, NatV;
  std::vector<Ttriangle> Tr; 
  
  if (!march.triangulize(delta, max_triangles, V, NatV, Tr)){
    std::cerr << "There is too much triangles\n";
  }
  
  end = clock();
  
  std::cout << " time=" << end - start << " um\n";
  std::cout << "V.size=" << V.size() << " T.size=" << Tr.size() << '\n';
  

  //
  // Calc triangle properties
  //
  
  std::cout << "Triangle properties:";
  
  start = clock();
    
  std::vector<T3Dpoint<double>> N;
    
  mesh_attributes(V, NatV, Tr, (std::vector<double>*)0, &N);
  
  end = clock();
  
  std::cout << " time= " << end - start << " um\n";
  std::cout << "N.size=" << N.size() << '\n';
  
  //
  //  Testing the new eclipsing algorithm
  //
  
  std::cout << "New eclipsing:";
  
  start = clock();  
  
  std::vector<double> M;
  
  double 
    theta = 20./180*M_PI, 
    view[3] = {std::cos(theta), 0, std::sin(theta)};
    
  triangle_mesh_visibility(view, V, Tr, N, M);

  end = clock();
  
  std::cout << " time= " << end - start << " um\n";


  //
  // Storing results
  //
  
  std::cout << "Storing results:";
  
  start = clock();
  
  //
  // Viewer direction
  //
  std::ofstream fr("view_new.dat");
  fr.precision(16);
  for (int i = 0; i < 3; ++i) fr << view[i] << '\n';  
  fr.close();
  
  //
  // Save triangles
  //
  fr.open("triangles_new.dat");
  for (auto && t: Tr)
    for (int i = 0; i < 3; ++i) fr << V[t.indices[i]] << '\n';
  fr.close();
  
  //
  // Saving mask
  //
  fr.open("mask_new.dat");
  for (auto && m : M) fr << m << '\n';
  fr.close();
  
  end = clock();
  std::cout << " time= " << end - start << " um\n";

  
  return 0;
}