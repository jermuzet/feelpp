/* -*- mode: c++; coding: utf-8; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; show-trailing-whitespace: t -*- vim:fenc=utf-8:ft=tcl:et:sw=4:ts=4:sts=4

  This file is part of the Feel library

  Author(s): Christophe Prud'homme <christophe.prudhomme@ujf-grenoble.fr>
       Date: 2009-03-04

  Copyright (C) 2009 Universite Joseph Fourier (Grenoble I)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3.0 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
/**
   \file convection_other.cpp
   \author Christophe Prud'homme <christophe.prudhomme@ujf-grenoble.fr>
   \author Elisa Schenone
   \date 2012-08-13
 */
#include <boost/lexical_cast.hpp>

#include "convection_crb.hpp"

// Gmsh geometry/mesh generator
#include <feel/feelfilters/gmsh.hpp>

// gmsh importer
#include <feel/feelfilters/gmsh.hpp>


// ****** CONSTRUCTEURS ****** //

Convection_crb::Convection_crb( )
:
M_backend( backend_type::build( BACKEND_PETSC ) ),
exporter( Exporter<mesh_type>::New( "ensight" ) ),
M_Dmu( new parameterspace_type )
{
    this->init();
}

Convection_crb::Convection_crb( po::variables_map const& vm )
:
M_vm( vm ),
M_backend( backend_type::build( vm ) ),
exporter( Exporter<mesh_type>::New( vm, "convection" ) ),
M_Dmu( new parameterspace_type )
{
//    this->init();
}

// <int Order_s, int Order_p, int Order_t>
Feel::gmsh_ptrtype
Convection_crb::createMesh()
{

    timers["mesh"].first.restart();
    gmsh_ptrtype gmshp( new gmsh_type );
    gmshp->setWorldComm( Environment::worldComm() );

    double h = this->vm()["hsize"]. as<double>();
    double l = this->vm()["length"]. as<double>();

    std::ostringstream ostr;

    ostr << gmshp->preamble()
         << "a=" << 0 << ";\n"
         << "b=" << l << ";\n"
         << "c=" << 0 << ";\n"
         << "d=" << 1 << ";\n"
         << "hBis=" << h << ";\n"
         << "Point(1) = {a,c,0.0,hBis};\n"
         << "Point(2) = {b,c,0.0,hBis};\n"
         << "Point(3) = {b,d,0.0,hBis};\n"
         << "Point(4) = {a,d,0.0,hBis};\n"
         << "Point(5) = {b/2,d,0.0,hBis};\n"
         << "Point(6) = {b/2,d/2,0.0,hBis};\n"
         << "Line(1) = {1,2};\n"
         << "Line(2) = {2,3};\n"
         << "Line(3) = {3,5};\n"
         << "Line(4) = {5,4};\n"
         << "Line(5) = {4,1};\n"
         << "Line(6) = {5,6};\n"
         << "Line Loop(7) = {1,2,3,4,5,6};\n"
         //<< "Line Loop(7) = {1,2,3,4,5};\n"
         << "Plane Surface(8) = {7};\n";
#if CONVECTION_DIM == 2
    ostr << "Physical Line(\"Tinsulated\") = {1,3,4};\n"
         << "Physical Line(\"Tfixed\") = {5};\n"
         << "Physical Line(\"Tflux\") = {2};\n"
        // << "Physical Line(\"Fflux\") = {6};\n"
         << "Physical Line(\"F.wall\") = {3, 4, 5, 1, 2};\n"
         << "Physical Surface(\"domain\") = {8};\n";
#else
    ostr << "Extrude {0, 0, 1} {\n"
         << "   Surface{8};\n"
         << "}\n"
         << "Physical Surface(\"Tfixed\") = {35};\n"
         << "Physical Surface(\"Tflux\") = {23};\n"
         << "Physical Surface(\"Tinsulated\") = {19, 40, 8, 31, 27};\n"
        //<< "Physical Surface(\"Fflux\") = {39};\n"
         << "Physical Surface(\"F.wall\") = {31, 27, 23, 19, 35, 40, 8};\n"
         << "Physical Volume(\"domain\") = {1};\n";
#endif
    std::ostringstream fname;
    fname << "domain";

    gmshp->setPrefix( fname.str() );
    gmshp->setDescription( ostr.str() );
    Log() << "[timer] createMesh(): " << timers["mesh"].second << "\n";

    return gmshp;

}



void
Convection_crb::exportResults( element_type& U )
{
    exporter->step( 0 )->setMesh( U.functionSpace()->mesh() );
    exporter->step( 0 )->add( "u", U. element<0>() );
    exporter->step( 0 )->add( "p", U. element<1>() );
    exporter->step( 0 )->add( "T", U. element<2>() );
    exporter->save();

}

// <int Order_s, int Order_p, int Order_t>
void Convection_crb ::exportResults( element_type& U, double t )
{
    exporter->step( t )->setMesh( U.functionSpace()->mesh() );
    exporter->step( t )->add( "u", U. element<0>() );
    exporter->step( t )->add( "p", U. element<1>() );
    exporter->step( t )->add( "T", U. element<2>() );
    exporter->save();
}

void
Convection_crb::solve( sparse_matrix_ptrtype& D,
              element_type& u,
              vector_ptrtype& F )
{
    
    vector_ptrtype U( M_backend->newVector( u.functionSpace() ) );
    M_backend->solve( D, D, U, F );
    u = *U;
}

void
Convection_crb::solve( parameter_type const& mu )
{
    element_ptrtype T( new element_type( Xh ) );
    this->solve( mu, T );
    
}

void
Convection_crb::solve( parameter_type const& mu, element_ptrtype& T )
{
    using namespace vf;
    Feel::ParameterSpace<2>::Element M_current_mu( mu );

    M_backend->nlSolver()->jacobian = boost::bind( &self_type::updateJacobian, boost::ref( *this ), _1, _2 );
    
    vector_ptrtype R( M_backend->newVector( Xh ) );
    sparse_matrix_ptrtype J( M_backend->newMatrix( Xh,Xh ) );
    
    double gr = mu( 0 );
    double pr = mu( 1 );
    
    int N=std::max( 1.0,std::max( std::ceil( std::log( gr ) ),std::ceil( std::log( pr )-std::log( 1.e-2 ) ) ) );
    
    for ( int i = 0; i < N; ++i )
    {
        int denom = ( N==1 )?1:N-1;
        M_current_Grashofs = math::exp( math::log( 1. )+i*( math::log( gr )-math::log( 1. ) )/denom );
        M_current_Prandtl = math::exp( math::log( 1.e-2 )+i*( math::log( pr )-math::log( 1.e-2 ) )/denom );

        std::cout << "i/N = " << i << "/" << N <<std::endl;
        std::cout << " intermediary Grashof = " << M_current_Grashofs<<std::endl;
        std::cout<< " and Prandtl = " << M_current_Prandtl << "\n"<<std::endl;
        
        M_current_mu << M_current_Grashofs, M_current_Prandtl;
        M_backend->nlSolver()->residual = boost::bind( &self_type::updateResidual, boost::ref( *this ), _1, _2, M_current_mu );
        
        this->computeThetaq( M_current_mu );
        this->update( M_current_mu );
        
        M_backend->nlSolve(_jacobian=J , _solution=T);
            
        if ( exporter->doExport() )
        {
            this->exportResults( *T, i );
        }
    }
}

void
Convection_crb::l2solve( vector_ptrtype& u, vector_ptrtype const& f )
{
    M_backend->solve( _matrix=M,  _solution=u, _rhs=f );
}

double
Convection_crb::scalarProduct( vector_ptrtype const& x, vector_ptrtype const& y )
{
    return M->energy( x, y );
}
double
Convection_crb::scalarProduct( vector_type const& x, vector_type const& y )
{
    return M->energy( x, y );
}

void
Convection_crb::run( const double * X, unsigned long N, double * Y, unsigned long P )
{
    
    std::cout << "\nConvection_crb::run( const double * X, unsigned long N, double * Y, unsigned long P )\n\n";
    
    
/*    using namespace vf;
    Feel::ParameterSpace<2>::Element mu( M_Dmu );
    mu << X[0], X[1];
    static int do_init = true;
    
    if ( do_init )
    {
//        double meshSize = X[2];
        this->init();
        do_init = false;
    }
    
    this->solve( mu, pT );
 */   
//    double mean = integrate( elements( mesh ), chi( ( Px() >= -0.1 ) && ( Px() <= 0.1 ) )*idv( *pT ) ).evaluate()( 0,0 )/0.2;
//    Y[0]=mean;

}



double
Convection_crb::output( int output_index, parameter_type const& mu )
{
    using namespace vf;
    this->solve( mu, pT );
    
    auto mesh = Xh->mesh();
    auto U = Xh->element( "u" );
    U = *pT;

    element_2_type t = U. element<2>(); // fonction temperature
    
    double output = 0.0;
    
    // right hand side (compliant)
    if ( output_index == 0 )
    {
        output = 0.0 ;

    }
    
    // output
    if ( output_index == 1 )
    {
        output = integrate( markedfaces( mesh, "Tflux" ) , idv( t ) ).evaluate()( 0,0 ) ;

        double AverageTdomain = integrate( elements( mesh ) , idv( t ) ).evaluate()( 0,0 ) ;
        std::cout << "AverageTdomain = " << AverageTdomain << std::endl;

    }
    
    return output;
    
}

// instantiation
// class Convection_crb<2,1,2>;