#include "VectorPatch.h"

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>

#include "Hilbert_functions.h"
#include "PatchesFactory.h"
#include "Species.h"
#include "Particles.h"
#include "SmileiIOFactory.h"
#include <cstring>
//#include <string>

using namespace std;

VectorPatch::VectorPatch()
{
}

VectorPatch::~VectorPatch()
{
}

void VectorPatch::exchangeParticles(int ispec, Params &params, SmileiMPI* smpi)
{
    #pragma omp for schedule(runtime)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
      (*this)(ipatch)->initExchParticles(smpi, ispec, params);
    }

    //cout << "init exch done" << endl;

    // Per direction
    for (unsigned int iDim=0 ; iDim<params.nDim_particle ; iDim++) {
        //cout << "initExchParticles done for " << iDim << endl;
        #pragma omp for schedule(runtime)
        for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
            (*this)(ipatch)->initCommParticles(smpi, ispec, params, iDim, this);
        }
        #pragma omp for schedule(runtime)
        for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
            (*this)(ipatch)->CommParticles(smpi, ispec, params, iDim, this);
        }
        //cout << "init comm done for dim " << iDim << endl;
        //cout << "initCommParticles done for " << iDim << endl;
        #pragma omp for schedule(runtime)
        for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
            (*this)(ipatch)->finalizeCommParticles(smpi, ispec, params, iDim, this);
        }
        //cout << "final comm done for dim " << iDim << endl;
    }

    #pragma omp for schedule(runtime)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->vecSpecies[ispec]->sort_part();

}

void VectorPatch::sumRhoJ(unsigned int diag_flag )
{

    //#pragma omp single
    //{
    //    Jx_.resize(this->size());
    //    Jy_.resize(this->size());
    //    Jz_.resize(this->size());
    //    rho_.resize(this->size());
    //}
    #pragma omp for schedule(static)
    for (int ipatch=0 ; ipatch<this->size() ; ipatch++){
        Jx_[ipatch]= (*this)(ipatch)->EMfields->Jx_ ;
        Jy_[ipatch]= (*this)(ipatch)->EMfields->Jy_ ;
        Jz_[ipatch]= (*this)(ipatch)->EMfields->Jz_ ;
        rho_[ipatch]= (*this)(ipatch)->EMfields->rho_ ;
    }
    sum( Jx_ );
    sum( Jy_ );
    sum( Jz_ );
    if(diag_flag) sum( rho_ );
}

void VectorPatch::sumRhoJs( int ispec )
{
    //#pragma omp single
    //{
    //    Jx_.resize(this->size());
    //    Jy_.resize(this->size());
    //    Jz_.resize(this->size());
    //    rho_.resize(this->size());
    //}
    #pragma omp for schedule(static)
    for (int ipatch=0 ; ipatch<this->size() ; ipatch++) {
        Jx_[ipatch]= (*this)(ipatch)->EMfields->Jx_s[ispec] ;
        Jy_[ipatch]= (*this)(ipatch)->EMfields->Jy_s[ispec] ;
        Jz_[ipatch]= (*this)(ipatch)->EMfields->Jz_s[ispec] ;
        rho_[ipatch]= (*this)(ipatch)->EMfields->rho_s[ispec] ;
    }

    sum( Jx_ );
    sum( Jy_ );
    sum( Jz_ );
    sum( rho_ );
}

void VectorPatch::exchangeE( )
{
    //#pragma omp single
    //{
    //    Ex_.resize(this->size());
    //    Ey_.resize(this->size());
    //    Ez_.resize(this->size());
    //}
    #pragma omp for schedule(static)
    for (int ipatch=0 ; ipatch<this->size() ; ipatch++) {
        Ex_[ipatch]= (*this)(ipatch)->EMfields->Ex_ ;
        Ey_[ipatch]= (*this)(ipatch)->EMfields->Ey_ ;
        Ez_[ipatch]= (*this)(ipatch)->EMfields->Ez_ ;
    }

    exchange( Ex_ );
    exchange( Ey_ );
    exchange( Ez_ );
}

void VectorPatch::exchangeB( )
{
    //#pragma omp single
    //{
    //    Bx_.resize(this->size());
    //    By_.resize(this->size());
    //    Bz_.resize(this->size());
    //}
    #pragma omp for schedule(static)
    for (int ipatch=0 ; ipatch<this->size() ; ipatch++) {
        Bx_[ipatch]= (*this)(ipatch)->EMfields->Bx_ ;
        By_[ipatch]= (*this)(ipatch)->EMfields->By_ ;
        Bz_[ipatch]= (*this)(ipatch)->EMfields->Bz_ ;
    }

    if ( Bx_[0]->dims_.size()>1 ) {
	exchange1( Bx_ );
	exchange0( By_  );
	exchange ( Bz_ );
    }
    else if (Bx_[0]->dims_.size()==1) {
	exchange0( By_  );
	exchange0( Bz_ );
    }

}

void VectorPatch::computeGlobalDiags(int timestep)
{
    
    computeScalarsDiags(timestep);
    //computeGlobalDiags(probes); // HDF5 write done per patch in DiagProbes::*
    computePhaseSpace();
    computeParticlesDiags(timestep);
    
}

void VectorPatch::computeScalarsDiags(int timestep)
{
    //cout << "In Global Compute Scalar Diags " << (*this)(0)->Diags->scalars.every << " \t timestep = " << timestep << endl;
    int scalars_every( (*this)(0)->Diags->scalars.every );
    if (timestep % scalars_every != 0) return;

    //cout << "In Global Compute Scalar Daigs\n";

    //std::vector<std::pair<std::string,double> > out_list;
    //std::vector<std::string> out_key;
    //std::vector<double>      out_value;
    //std::vector<unsigned int> out_width;
    //std::vector<std::pair<std::string,double> >::iterator itDiagScalar;


    int nDiags( (*this)(0)->Diags->scalars.out_value.size() );
    // Initialize scalars iterator on 1st diag
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->Diags->scalars.itDiagScalarName  =  (*this)(ipatch)->Diags->scalars.out_key.begin();
	(*this)(ipatch)->Diags->scalars.itDiagScalarValue =  (*this)(ipatch)->Diags->scalars.out_value.begin();
    }


    for (int idiags = 0 ; idiags<nDiags ; idiags++) {
	string diagName( *(*this)(0)->Diags->scalars.itDiagScalarName );

	if ( ( diagName.find("Min") == std::string::npos ) && ( diagName.find("Max") == std::string::npos ) ) {
	    double sum(0.);
	    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
		sum += *(*this)(ipatch)->Diags->scalars.itDiagScalarValue;
		if (ipatch) {
		    (*this)(ipatch)->Diags->scalars.itDiagScalarName++;
		    (*this)(ipatch)->Diags->scalars.itDiagScalarValue++;
		}
	    }
	    *(*this)(0)->Diags->scalars.itDiagScalarValue = sum;
	    (*this)(0)->Diags->scalars.itDiagScalarName++;
	    (*this)(0)->Diags->scalars.itDiagScalarValue++;
	}
	else if ( diagName.find("MinCell") != std::string::npos ) {
	    vector<double>::iterator iterVal    = (*this)(0)->Diags->scalars.itDiagScalarValue-1;
	    vector<double>::iterator iterValRef = (*this)(0)->Diags->scalars.itDiagScalarValue-1;
	    double min( *iterValRef );

	    for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++) {
		if ( *(*this)(ipatch)->Diags->scalars.itDiagScalarValue < min ) {
		    min = *(*this)(ipatch)->Diags->scalars.itDiagScalarValue;
		    iterVal = (*this)(ipatch)->Diags->scalars.itDiagScalarValue-1;
		}
		if (ipatch) {
		    (*this)(ipatch)->Diags->scalars.itDiagScalarName++;
		    (*this)(ipatch)->Diags->scalars.itDiagScalarValue++;
		}
	    }
	    *(*this)(0)->Diags->scalars.itDiagScalarValue = min;
	    iterValRef = iterVal;

	    (*this)(0)->Diags->scalars.itDiagScalarName++;	    
	    (*this)(0)->Diags->scalars.itDiagScalarValue++;	    
	}
	else if ( diagName.find("MaxCell") != std::string::npos ) {
	    vector<double>::iterator iterVal    = (*this)(0)->Diags->scalars.itDiagScalarValue-1;
	    vector<double>::iterator iterValRef = (*this)(0)->Diags->scalars.itDiagScalarValue-1;
	    double max( *iterValRef );

	    for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++) {
		if ( *(*this)(ipatch)->Diags->scalars.itDiagScalarValue > max ) {
		    max = *(*this)(ipatch)->Diags->scalars.itDiagScalarValue;
		    iterVal = (*this)(ipatch)->Diags->scalars.itDiagScalarValue-1;
		}
		if (ipatch) {
		    (*this)(ipatch)->Diags->scalars.itDiagScalarName++;
		    (*this)(ipatch)->Diags->scalars.itDiagScalarValue++;
		}
	    }
	    *(*this)(0)->Diags->scalars.itDiagScalarValue = max;
	    iterValRef = iterVal;

	    (*this)(0)->Diags->scalars.itDiagScalarName++;	    
	    (*this)(0)->Diags->scalars.itDiagScalarValue++;	    
	}

	// Go to next diag
    }

    // After MPI sync
    //(*this)(0)->Diags->scalars.write(timestep);

}

void VectorPatch::computePhaseSpace()
{
    // A définir : DiagPhaseSpace::itDiagPhase

    int nDiags( (*this)(0)->Diags->phases.vecDiagPhaseToRun.size() );

    // Initialize scalars iterator on 1st diag
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->Diags->phases.itDiagPhase =  (*this)(ipatch)->Diags->phases.vecDiagPhaseToRun.begin();
    
    for (int idiags = 0 ; idiags<nDiags ; idiags++) {
	vector<unsigned int> diagSize = (*(*this)(0)->Diags->phases.itDiagPhase)->my_data.dims_;
	for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++) {
	    for (int i=0 ; i<diagSize[0] ; i++)
		for (int j=0 ; j<diagSize[1] ; j++)
		    (*(*this)(0)->Diags->phases.itDiagPhase)->my_data(i,j) += (*(*this)(ipatch)->Diags->phases.itDiagPhase)->my_data(i,j);
	    (*this)(ipatch)->Diags->phases.itDiagPhase++;
	} // for ipatch
	(*this)(0)->Diags->phases.itDiagPhase++;

    } // for idiags

    for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->Diags->phases.vecDiagPhaseToRun.clear();

}


void VectorPatch::computeParticlesDiags(int timestep)
{
    int nDiags( (*this)(0)->Diags->vecDiagnosticParticles.size() );

    for (int idiags = 0 ; idiags<nDiags ; idiags++) {
	if (timestep % (*this)(0)->Diags->vecDiagnosticParticles[idiags]->every != (*this)(0)->Diags->vecDiagnosticParticles[idiags]->time_average-1) continue;

	int output_size = (*this)(0)->Diags->vecDiagnosticParticles[idiags]->output_size;
	for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++) {
	    for (int i=0 ; i<output_size ; i++)
		(*this)(0)->Diags->vecDiagnosticParticles[idiags]->data_sum[i] += (*this)(ipatch)->Diags->vecDiagnosticParticles[idiags]->data_sum[i];
	} // for ipatch

    } // for idiags

    
    for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++)
	for (unsigned int i=0; i<(*this)(ipatch)->Diags->vecDiagnosticParticles.size(); i++)
	       if ((*this)(ipatch)->Diags->vecDiagnosticParticles[i]->time_average == 1)
		   (*this)(ipatch)->Diags->vecDiagnosticParticles[i]->clean();

}

void VectorPatch::initProbesDiags(Params& params, int timestep)
{
    (*this)(0)->Diags->probes.createFile();
    // Start at 0, cause of setFile set probesStart (locate writing point in h5 file)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->Diags->probes.setFile( (*this)(0)->Diags->probes.fileId, (*this)(ipatch), params );
    }
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->Diags->probes.waitSetFile( params );
    }    //cout << " File created " << endl;

    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	//cout << "Data written for " << ipatch << endl;
	(*this)(ipatch)->Diags->probes.writePositionIn(params);
	//cout << "End of Data written for " << ipatch << endl;
    }
}

void VectorPatch::finalizeProbesDiags(Params& params, int timestep)
{
    for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->Diags->probes.setFile( 0 );
    }

}

void VectorPatch::initDumpFields(Params& params, int timestep)
{
    (*this)(0)->sio->createFiles(params, (*this)(0));
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->sio->setFiles( (*this)(0)->sio->global_file_id_, (*this)(0)->sio->global_file_id_avg );
    }
}

void VectorPatch::finalizeDumpFields(Params& params, int timestep)
{
    for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->sio->setFiles( 0, 0 );
    }

}

void VectorPatch::initTrackParticles(Params& params, SmileiMPI* smpi)
{
    int nspecies = (*this)(0)->vecSpecies.size();
    int idiag(0);
    for ( int ispec=0 ; ispec<nspecies ; ispec++) {
	
	// Communicate some stuff if this is a species that has to be dumped (particles have Id)
	// Need to be placed after ALL createParticles()
	if ((*this)(0)->vecSpecies[ispec]->particles->track_every) {

	    // Internal patches offset

	    std::vector<int> localNbrParticles( this->size(), 0 );
	    localNbrParticles[0] = (*this)(0)->vecSpecies[ispec]->getNbrOfParticles();
	    for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++) {
		// number of particles up to ipatch (including)
		localNbrParticles[ipatch] += (*this)(ipatch)->vecSpecies[ispec]->getNbrOfParticles() + localNbrParticles[ipatch-1];
		(*this)(ipatch)->vecSpecies[ispec]->particles->addIdOffsets(localNbrParticles[ipatch-1]);
	    }
	    int locNbrParticles = localNbrParticles[this->size()-1];


	    // MPI offset

	    //int locNbrParticles = thisSpecies->getNbrOfParticles();
	    int sz(1);
	    MPI_Comm_size( MPI_COMM_WORLD, &sz );
	    std::vector<int> allNbrParticles(sz);
	    MPI_Allgather( &locNbrParticles, 1, MPI_INTEGER, &allNbrParticles[0], 1, MPI_INTEGER, MPI_COMM_WORLD );

	    int totNbrParts(0);
	    for (int irk=0 ; irk<sz ; irk++) totNbrParts += allNbrParticles[irk];
	    // HDF5 file open by all patch master
	    (*this)(0)->Diags->vecDiagnosticTrackParticles[idiag]->createFile(totNbrParts,params);

	    // Set HDF5 context for other patches
	    for (unsigned int ipatch=1 ; ipatch<this->size() ; ipatch++)
		(*this)(ipatch)->Diags->vecDiagnosticTrackParticles[idiag]->setGlobalNbrParticles(totNbrParts);

	    idiag++; // Considered DiagnosticTrackParticles ordered as Species

	    int nParticles(0);

	    nParticles =  allNbrParticles[0];
	    for (int irk=1 ; irk<sz ; irk++){
		allNbrParticles[irk] += nParticles;
		nParticles = allNbrParticles[irk];
	    }
	    for (int irk=sz-1 ; irk>0 ; irk--){
		allNbrParticles[irk] = allNbrParticles[irk-1];
	    }
	    allNbrParticles[0] = 0;

	    int offset(0);
	    MPI_Scatter(&allNbrParticles[0], 1 , MPI_INTEGER, &offset, 1, MPI_INTEGER, 0, MPI_COMM_WORLD );
	    
	    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
		(*this)(ipatch)->vecSpecies[ispec]->particles->addIdOffsets(offset);

	} // End if track_every

	// Count total number of track particles (need to define HDF5 context)
	
	//MPI_Bcast();

    } // End for ispec

} // End initTrackParticles



void VectorPatch::initCollisionDebug()
{
    for (unsigned int icoll=0 ; icoll<(*this)(0)->vecCollisions.size(); icoll++)
	(*this)(0)->vecCollisions[icoll]->createFile();

} // End initCollisionDebug

void VectorPatch::createPatches(Params& params, SmileiMPI* smpi, SimWindow* simWindow)
{
    unsigned int n_moved(0), nPatches_now;
    recv_patches_.resize(0);

    // Set Index of the 1st patch of the vector yet on current MPI rank
    // Is this really necessary ? It should be done already ...
    refHindex_ = (*this)(0)->Hindex();

    //When going to openMP, these two vectors must be stored by patch and not by vectorPatch.
    recv_patch_id_.clear();
    send_patch_id_.clear();
    
    
    // define recv_patches_ parsing patch_count
    // Go to 1st patch to recv (maybe yet on current CPU)
    // istart = Index of the futur 1st patch
    // recv : store real Hindex
    int istart( 0 );
    for (int irk=0 ; irk<smpi->getRank() ; irk++) istart += smpi->patch_count[irk];
    //recv_patch_id stores all the hindex this process must own at the end of the exchange.
    for (int ipatch=0 ; ipatch<smpi->patch_count[smpi->getRank()] ; ipatch++)
	recv_patch_id_.push_back( istart+ipatch );


    // define send_patches_ parsing patch_count
    // send_patch_id_ stores indices from 0 to current npatch(before exchange)
    //for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
    //    send_patch_id_.push_back( ipatch );
    //}
    //Current number of patch
    nPatches_now = this->size() ;


    //std::vector<int> tmp(0);
    //Loop on current patches...
    //for (unsigned int ipatch=0 ; ipatch<send_patch_id_.size() ; ipatch++)
    for (unsigned int ipatch=0 ; ipatch < nPatches_now ; ipatch++)
      //if        current hindex        <  future refHindex  OR current hindex > future last hindex...
	if ( ( refHindex_+ipatch < recv_patch_id_[0] ) || ( refHindex_+ipatch > recv_patch_id_.back() ) )
      //    put this patch in tmp. We will have to send it away.
	    //tmp.push_back( ipatch );
	    send_patch_id_.push_back( ipatch );

    //  nPatches <- future number of patches owned.
    // Backward loop on future patches...
    for ( int ipatch=recv_patch_id_.size()-1 ; ipatch>=0 ; ipatch--) {
      //if      future patch hindex  >= current refHindex             AND    future patch hindex <= current last hindex
	//if ( ( recv_patch_id_[ipatch]>=refHindex_+send_patch_id_[0] ) && ( recv_patch_id_[ipatch]<=refHindex_+send_patch_id_[send_patch_id_.size()-1] ) ) {
	//                                          send_patch_id_[0] should be equal to 0 ??
	if ( ( recv_patch_id_[ipatch]>=refHindex_ ) && ( recv_patch_id_[ipatch] <= refHindex_ + nPatches_now - 1 ) ) {
            //Remove this patch from the receive list because I already own it.
	    recv_patch_id_.erase( recv_patch_id_.begin()+ipatch );
	}
    }

    //send_patch_id_ = tmp;

    if (simWindow) n_moved = simWindow->getNmoved(); 
    // Store in local vector future patches
    // Loop on the patches I have to receive and do not already own.
    for (unsigned int ipatch=0 ; ipatch < recv_patch_id_.size() ; ipatch++) {
	// density profile is initializes as if t = 0 !
	// Species will be cleared when, nbr of particles will be known
        //Creation of a new patch, ready to receive its content from MPI neighbours.
	Patch* newPatch = PatchesFactory::create(params, smpi, recv_patch_id_[ipatch], n_moved );
        //Store pointers to newly created patch in recv_patches_.
	recv_patches_.push_back( newPatch );
    }

}

void VectorPatch::setNbrParticlesToExch(SmileiMPI* smpi)
{
    int nSpecies( (*this)(0)->vecSpecies.size() );
    int nDim_Parts( (*this)(0)->vecSpecies[0]->particles->dimension() );

    // Send particles
    for (unsigned int ipatch=0 ; ipatch<send_patch_id_.size() ; ipatch++) {

	int newMPIrank(0);
	// locate rank which will own send_patch_id_[ipatch]
	int tmp( smpi->patch_count[newMPIrank] );
	while ( tmp <= send_patch_id_[ipatch]+refHindex_ ) {
	    newMPIrank++;
	    tmp += smpi->patch_count[newMPIrank];
	}

	vector<int> nbrOfPartsSend(nSpecies,0);
	for (int ispec=0 ; ispec<nSpecies ; ispec++) {
	    nbrOfPartsSend[ispec] = (*this)(send_patch_id_[ipatch])->vecSpecies[ispec]->getNbrOfParticles();
	}
#ifdef _DEBUGPATCH
	cout << smpi->getRank() << " send to " << newMPIrank << " with tag " << refHindex_+send_patch_id_[ipatch] << endl;
	for (int ispec=0;ispec<nSpecies;ispec++)
	  cout << "n part send = " << nbrOfPartsSend[ispec] << endl;
#endif
	smpi->send( nbrOfPartsSend, newMPIrank, refHindex_+send_patch_id_[ipatch] );
    }


    // Recv part
    for (unsigned int ipatch=0 ; ipatch<recv_patch_id_.size() ; ipatch++) {

	vector<int> nbrOfPartsRecv(nSpecies,0);
	int oldMPIrank(0); // Comparing recv_patch_id_[ipatch] to 1st yet on current MPI rank
	if ( recv_patch_id_[ipatch] > refHindex_ )
	    oldMPIrank = smpi->getRank()+1;
	else
	    oldMPIrank = smpi->getRank()-1;

#ifdef _DEBUGPATCH
	cout << smpi->getRank() << " recv from " << oldMPIrank << " with tag " << recv_patch_id_[ipatch] << endl;
	for (int ispec=0;ispec<nSpecies;ispec++)
	  cout << "n part recv = " << nbrOfPartsRecv[ispec] << endl;
#endif
	smpi->recv( &nbrOfPartsRecv, oldMPIrank, recv_patch_id_[ipatch] );
#ifdef _DEBUGPATCH
	for (int ispec=0;ispec<nSpecies;ispec++)
	  cout << "n part recv = " << nbrOfPartsRecv[ispec] << endl;
#endif
	for (int ispec=0 ; ispec<nSpecies ; ispec++)
	    recv_patches_[ipatch]->vecSpecies[ispec]->particles->initialize( nbrOfPartsRecv[ispec], (*(*this)(0)->vecSpecies[0]->particles) );
    }

    //Synchro, send/recv must be non-blocking !!!
    smpi->barrier();
}


//void VectorPatch::exchangePatches(SmileiMPI* smpi)
//{
//    int nSpecies( (*this)(0)->vecSpecies.size() );
//
//    // Send part
//    for (unsigned int ipatch=0 ; ipatch<send_patch_id_.size() ; ipatch++) {
//
//	int newMPIrank(0);
//	// locate rank which owns send_patch_id_[ipatch]
//	int tmp( smpi->patch_count[newMPIrank] );
//	while ( tmp <= send_patch_id_[ipatch]+refHindex_ ) {
//	    newMPIrank++;
//	    tmp += smpi->patch_count[newMPIrank];
//	}
//#ifdef _DEBUGPATCH
//	cout << smpi->getRank() << " send to " << newMPIrank << " with tag " << send_patch_id_[ipatch] << endl;
//#endif
//	smpi->send( (*this)(send_patch_id_[ipatch]), newMPIrank, refHindex_+send_patch_id_[ipatch] );
//
//    }
//
//
//    // Recv part
//    // recv_patch_id_ must be sorted !
//    // Loop / This, check this->hindex is/not recv_patch_id
//    for (unsigned int ipatch=0 ; ipatch<recv_patch_id_.size() ; ipatch++) {
//	int oldMPIrank(0); // Comparing recv_patch_id_[ipatch] to 1st yet on current MPI rank
//	if ( recv_patch_id_[ipatch] > refHindex_ )
//	    oldMPIrank = smpi->getRank()+1;
//	else
//	    oldMPIrank = smpi->getRank()-1;
//#ifdef _DEBUGPATCH
//	cout << smpi->getRank() << " recv from " << oldMPIrank << " with tag " << recv_patch_id_[ipatch] << endl;
//#endif
//	smpi->recv( recv_patches_[ipatch], oldMPIrank, recv_patch_id_[ipatch] );
//    }
//
//    //Synchro, send/recv must be non-blocking !!!
//
//    /*for (unsigned int ipatch=0 ; ipatch<send_patch_id_.size() ; ipatch++) {
//	delete (*this)(send_patch_id_[ipatch]-refHindex_);
//	patches_[ send_patch_id_[ipatch]-refHindex_ ] = NULL;
//	patches_.erase( patches_.begin() + send_patch_id_[ipatch] - refHindex_ );
//	
//    }*/
//    int nPatchSend(send_patch_id_.size());
//    for (int ipatch=nPatchSend-1 ; ipatch>=0 ; ipatch--) {
//	//Ok while at least 1 old patch stay inon current CPU
//	(*this)(send_patch_id_[ipatch])->Diags->probes.setFile(0);
//	(*this)(send_patch_id_[ipatch])->sio->setFiles(0,0);
//	delete (*this)(send_patch_id_[ipatch]);
//	patches_[ send_patch_id_[ipatch] ] = NULL;
//	patches_.erase( patches_.begin() + send_patch_id_[ipatch] );
//	
//    }
//
//    for (unsigned int ipatch=0 ; ipatch<recv_patch_id_.size() ; ipatch++) {
//	if ( recv_patch_id_[ipatch] > refHindex_ )
//	    patches_.push_back( recv_patches_[ipatch] );
//	else
//	    patches_.insert( patches_.begin()+ipatch, recv_patches_[ipatch] );
//    }
//    recv_patches_.clear();
//
//#ifdef _DEBUGPATCH
//    cout << smpi->getRank() << " number of patches " << this->size() << endl;
//#endif
//    for (int ipatch=0 ; ipatch<patches_.size() ; ipatch++ ) { 
//	(*this)(ipatch)->updateMPIenv(smpi);
//    }
//
//    definePatchDiagsMaster();
//
//}
void VectorPatch::output_exchanges(SmileiMPI* smpi)
{
    ofstream output_file;
    ostringstream name("");
    name << "debug_output"<<smpi->smilei_rk<<".txt" ;
    output_file.open(name.str().c_str(), std::ofstream::out | std::ofstream::app);
    int newMPIrank, oldMPIrank;
    newMPIrank = smpi->smilei_rk -1;
    oldMPIrank = smpi->smilei_rk -1;
    int istart( 0 );
    for (int irk=0 ; irk<smpi->getRank() ; irk++) istart += smpi->patch_count[irk];
    for (unsigned int ipatch=0 ; ipatch < send_patch_id_.size() ; ipatch++) {
        if(send_patch_id_[ipatch]+refHindex_ > istart ) newMPIrank = smpi->smilei_rk + 1;
        output_file << "Rank " << smpi->smilei_rk << " sending patch " << send_patch_id_[ipatch]+refHindex_ << " to " << newMPIrank << endl; 
    }
    for (unsigned int ipatch=0 ; ipatch < recv_patch_id_.size() ; ipatch++) {
        if(recv_patch_id_[ipatch] > refHindex_ ) oldMPIrank = smpi->smilei_rk + 1;
        output_file << "Rank " << smpi->smilei_rk << " receiving patch " << recv_patch_id_[ipatch] << " from " << oldMPIrank << endl; 
    }
    output_file << "NEXT" << endl;
    output_file.close();
}

void VectorPatch::exchangePatches(SmileiMPI* smpi, Params& params)
{
    hid_t globalFile    = (*this)(0)->sio->global_file_id_;
    hid_t globalFileAvg = (*this)(0)->sio->global_file_id_avg;

    int nSpecies( (*this)(0)->vecSpecies.size() );
    int newMPIrank, oldMPIrank;
    //int newMPIrankbis, oldMPIrankbis, tmp;
    int nDim_Parts( (*this)(0)->vecSpecies[0]->particles->dimension() );
    newMPIrank = smpi->smilei_rk -1;
    oldMPIrank = smpi->smilei_rk -1;
    int istart( 0 );
    int nmessage = 2*nSpecies+10;


    std::vector<std::string> out_key;
    std::vector<double>      out_value;
    hid_t fphases;
    vector<hid_t> dset;

    if (smpi->isMaster()) {
	// Get scalars/phaseSpace patch 
	(*this)(0)->Diags->scalars.closeFile();
	fphases = (*this)(0)->Diags->phases.fileId;
	for ( int iphase=0 ; iphase<(*this)(0)->Diags->phases.vecDiagPhase.size() ; iphase++ ) {
	    dset.push_back( (*this)(0)->Diags->phases.vecDiagPhase[iphase]->dataId );
	}

	vector<string>::iterator iterKey = (*this)(0)->Diags->scalars.out_key.begin();
	for(vector<double>::iterator iter = (*this)(0)->Diags->scalars.out_value.begin(); iter !=(*this)(0)->Diags->scalars.out_value.end(); iter++) {
	    out_key.push_back( *iterKey );
	    iterKey++;
	    out_value.push_back( *iter );
	}

	
    }


    for (int irk=0 ; irk<smpi->getRank() ; irk++) istart += smpi->patch_count[irk];
    // Send part
    // Send particles
    for (unsigned int ipatch=0 ; ipatch < send_patch_id_.size() ; ipatch++) {
	// locate rank which will own send_patch_id_[ipatch]
	// We assume patches are only exchanged with neighbours.
	// Once all patches supposed to be sent to the left are done, we send the rest to the right.
      //if   hindex of patch to be sent              >  future hindex of the first patch owned by this process 
        if(send_patch_id_[ipatch]+refHindex_ > istart ) newMPIrank = smpi->smilei_rk + 1;
        //cout << "Rank " << smpi->smilei_rk << " sending patch " << send_patch_id_[ipatch]+refHindex_ << " to " << newMPIrank << endl; 
	//newMPIrankbis = 0 ;
	//tmp = smpi->patch_count[newMPIrankbis];
	//while ( tmp <= send_patch_id_[ipatch]+refHindex_ ) {
	//    newMPIrankbis++;
	//    tmp += smpi->patch_count[newMPIrankbis];
	//}
        
        //if (newMPIrank != newMPIrankbis){
        //    cout << "newMIPrank problem ! " << newMPIrank << endl;
        //    newMPIrank = newMPIrankbis ;
        //}

	smpi->isend( (*this)(send_patch_id_[ipatch]), newMPIrank, (refHindex_+send_patch_id_[ipatch])*nmessage );
    }

    for (unsigned int ipatch=0 ; ipatch < recv_patch_id_.size() ; ipatch++) {
      //if   hindex of patch to be received > first hindex actually owned, that means it comes from the next MPI process and not from the previous anymore. 
        if(recv_patch_id_[ipatch] > refHindex_ ) oldMPIrank = smpi->smilei_rk + 1;
        //cout << "Rank " << smpi->smilei_rk << " receiving patch " << recv_patch_id_[ipatch] << " from " << oldMPIrank << endl; 
	//oldMPIrankbis = 0 ; // Comparing recv_patch_id_[ipatch] to 1st yet on current MPI rank
	//if ( recv_patch_id_[ipatch] > refHindex_ )
	//    oldMPIrankbis = smpi->getRank()+1;
	//else
	//    oldMPIrankbis = smpi->getRank()-1;

        //if (oldMPIrank != oldMPIrankbis){
        //    cout << "oldMIPrank problem ! " << oldMPIrank << endl;
        //    oldMPIrank = oldMPIrankbis ;
        //}
        smpi->new_recv( recv_patches_[ipatch], oldMPIrank, recv_patch_id_[ipatch]*nmessage, params );
    }

    smpi->barrier();
    //Delete sent patches
    int nPatchSend(send_patch_id_.size());
    for (int ipatch=nPatchSend-1 ; ipatch>=0 ; ipatch--) {
	//Ok while at least 1 old patch stay inon current CPU
	(*this)(send_patch_id_[ipatch])->Diags->probes.setFile(0);
	(*this)(send_patch_id_[ipatch])->sio->setFiles(0,0);
	delete (*this)(send_patch_id_[ipatch]);
	patches_[ send_patch_id_[ipatch] ] = NULL;
	patches_.erase( patches_.begin() + send_patch_id_[ipatch] );
	
    }
    //Put received patches in the global vecPatches
    for (unsigned int ipatch=0 ; ipatch<recv_patch_id_.size() ; ipatch++) {
	if ( recv_patch_id_[ipatch] > refHindex_ )
	    patches_.push_back( recv_patches_[ipatch] );
	else
	    patches_.insert( patches_.begin()+ipatch, recv_patches_[ipatch] );
    }
    recv_patches_.clear();

    for (int ipatch=0 ; ipatch<patches_.size() ; ipatch++ ) { 
	(*this)(ipatch)->updateMPIenv(smpi);
    }

    //definePatchDiagsMaster();
    definePatchDiagsMaster( globalFile, globalFileAvg );
    updatePatchFieldDump( params );

    if (smpi->isMaster()) {
	vector<string>::iterator iterKey = out_key.begin();
	for(vector<double>::iterator iter = out_value.begin(); iter !=out_value.end(); iter++) {
	    (*this)(0)->Diags->scalars.out_key.push_back( *iterKey );
	    iterKey++;
	    (*this)(0)->Diags->scalars.out_value.push_back( *iter );
	}


        // Set scalars/phaseSpace patch master
        (*this)(0)->Diags->scalars.open(true);
        (*this)(0)->Diags->phases.fileId = fphases;
        for ( int iphase=0 ; iphase<(*this)(0)->Diags->phases.vecDiagPhase.size() ; iphase++ ) {
            (*this)(0)->Diags->phases.vecDiagPhase[iphase]->dataId = dset[ iphase ];
        }
    }
    (*this).set_refHindex() ;
    (*this).Diags = (*this)(0)->Diags;

    resizeFields();   

}

void VectorPatch::definePatchDiagsMaster(hid_t globalFile, hid_t globalFileAvg)
{
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
        (*this)(ipatch)->sio->setFiles( globalFile, globalFileAvg );
    }

}

void VectorPatch::definePatchDiagsMaster()
{
    int patchIdMaster(0);
    for (patchIdMaster=0 ; patchIdMaster<patches_.size() ; patchIdMaster++ )
	if ( (*this)(patchIdMaster)->Diags->probes.fileId != 0 ) break;

    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	if ((ipatch!=patchIdMaster) && (patchIdMaster!=patches_.size()) ) { // patchIdMaster!=patches_.size() 
		(*this)(ipatch)->Diags->probes.setFile( (*this)(patchIdMaster)->Diags->probes.fileId );
	}
    }

    for (patchIdMaster=0 ; patchIdMaster<patches_.size() ; patchIdMaster++ )
	if ( (*this)(patchIdMaster)->sio->global_file_id_ != 0 ) break;

    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	if ((ipatch!=patchIdMaster) && (patchIdMaster!=patches_.size()) ) { // patchIdMaster!=patches_.size() 
	    (*this)(ipatch)->sio->setFiles( (*this)(patchIdMaster)->sio->global_file_id_, (*this)(patchIdMaster)->sio->global_file_id_avg );
	}
    }

}

void VectorPatch::updatePatchFieldDump( Params& params )
{
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	if ( (*this)(ipatch)->Pcoordinates[0]!=params.number_of_patches[0]-1 )
	    (*this)(ipatch)->sio->updatePattern( params, (*this)(ipatch) );
    }
    
}


void VectorPatch::solvePoisson( Params &params, SmileiMPI* smpi )
{
    unsigned int nx_p2_global = (params.n_space_global[0]+1) * (params.n_space_global[1]+1);

    unsigned int iteration_max = 50000;
    double       error_max     = 1.e-14;
    unsigned int iteration=0;

    // Init & Store internal data (phi, r, p, Ap) per patch
    double rnew_dot_rnew_local(0.);
    double rnew_dot_rnew(0.);    
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	(*this)(ipatch)->EMfields->initPoisson( (*this)(ipatch) );
	rnew_dot_rnew_local += (*this)(ipatch)->EMfields->compute_r();
    }
    MPI_Allreduce(&rnew_dot_rnew_local, &rnew_dot_rnew, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

    Ap_.resize(0);
    for (int ipatch=0 ; ipatch<this->size() ; ipatch++)
	Ap_.push_back( (*this)(ipatch)->EMfields->Ap_ );

    // compute control parameter
    double ctrl = rnew_dot_rnew / (double)(nx_p2_global);
	
    // ---------------------------------------------------------
    // Starting iterative loop for the conjugate gradient method
    // ---------------------------------------------------------
    if (smpi->isMaster()) DEBUG("Starting iterative loop for CG method");
    while ( (ctrl > error_max) && (iteration<iteration_max) ) {
        
        iteration++;
        if (smpi->isMaster()) DEBUG("iteration " << iteration << " started with control parameter ctrl = " << ctrl*1.e14 << " x 1e-14");

        // scalar product of the residual
        double r_dot_r = rnew_dot_rnew;

	for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) 
	    (*this)(ipatch)->EMfields->compute_Ap( (*this)(ipatch) );

	// Exchange Ap_ (intra & extra MPI)
	exchange( Ap_ );

       // scalar product p.Ap
        double p_dot_Ap       = 0.0;
        double p_dot_Ap_local = 0.0;
	for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	    p_dot_Ap_local += (*this)(ipatch)->EMfields->compute_pAp();
	}
        MPI_Allreduce(&p_dot_Ap_local, &p_dot_Ap, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);


        // compute new potential and residual
	for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	    (*this)(ipatch)->EMfields->update_pand_r( r_dot_r, p_dot_Ap );
	}

        // compute new residual norm
        rnew_dot_rnew       = 0.0;
        rnew_dot_rnew_local = 0.0;
	for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	    rnew_dot_rnew_local += (*this)(ipatch)->EMfields->compute_r();
	}
	MPI_Allreduce(&rnew_dot_rnew_local, &rnew_dot_rnew, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        if (smpi->isMaster()) DEBUG("new residual norm: rnew_dot_rnew = " << rnew_dot_rnew);

        // compute new directio
	for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	    (*this)(ipatch)->EMfields->update_p( rnew_dot_rnew, r_dot_r );
	}

        // compute control parameter
        ctrl = rnew_dot_rnew / (double)(nx_p2_global);
        if (smpi->isMaster()) DEBUG("iteration " << iteration << " done, exiting with control parameter ctrl = " << ctrl);

    }//End of the iterative loop
    
    
    // --------------------------------
    // Status of the solver convergence
    // --------------------------------
    if (iteration == iteration_max) {
        if (smpi->isMaster())
            WARNING("Poisson solver did not converge: reached maximum iteration number: " << iteration
                    << ", relative error is ctrl = " << 1.0e14*ctrl << " x 1e-14");
    }
    else {
        if (smpi->isMaster()) 
            MESSAGE(1,"Poisson solver converged at iteration: " << iteration
                    << ", relative error is ctrl = " << 1.0e14*ctrl << " x 1e-14");
    }

    Ap_.clear();

    // ------------------------------------------
    // Compute the electrostatic fields Ex and Ey
    // ------------------------------------------
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->EMfields->initE( (*this)(ipatch) );

    exchangeE();    
    
    // Centering of the electrostatic fields
    // -------------------------------------

    vector<double> E_Add(Ex_[0]->dims_.size(),0.);
    if ( Ex_[0]->dims_.size()>1 ) {
	double Ex_WestNorth = 0.0;
	double Ey_WestNorth = 0.0;
	double Ex_EastSouth = 0.0;
	double Ey_EastSouth = 0.0;

	//The NorthWest patch has Patch coordinates X=0, Y=2^m1-1= number_of_patches[1]-1.
	//Its hindex is
	int patch_NorthWest = generalhilbertindex(params.mi[0], params.mi[1], 0,  params.number_of_patches[1]-1);
	//The MPI rank owning it is
	int rank_WestNorth = smpi->hrank(patch_NorthWest);
	//The SouthEast patch has Patch coordinates X=2^m0-1= number_of_patches[0]-1, Y=0.
	//Its hindex is
	int patch_SouthEast = generalhilbertindex(params.mi[0], params.mi[1], params.number_of_patches[0]-1, 0);
	//The MPI rank owning it is
	int rank_EastSouth = smpi->hrank(patch_SouthEast);


	//cout << params.mi[0] << " " << params.mi[1] << " " << params.number_of_patches[0] << " " << params.number_of_patches[1] << endl;
	//cout << patch_NorthWest << " " << rank_WestNorth << " " << patch_SouthEast << " " << rank_EastSouth << endl;

	if ( smpi->smilei_rk == rank_WestNorth ) {
	    Ex_WestNorth = (*this)(patch_NorthWest-((*this).refHindex_))->EMfields->getEx_WestNorth();
	    Ey_WestNorth = (*this)(patch_NorthWest-((*this).refHindex_))->EMfields->getEy_WestNorth();
	}
    
	// East-South corner
	if ( smpi->smilei_rk == rank_EastSouth ) {
	    Ex_EastSouth = (*this)(patch_SouthEast-((*this).refHindex_))->EMfields->getEx_EastSouth();
	    Ey_EastSouth = (*this)(patch_SouthEast-((*this).refHindex_))->EMfields->getEy_EastSouth();
	}

	MPI_Bcast(&Ex_WestNorth, 1, MPI_DOUBLE, rank_WestNorth, MPI_COMM_WORLD);
	MPI_Bcast(&Ey_WestNorth, 1, MPI_DOUBLE, rank_WestNorth, MPI_COMM_WORLD);

	MPI_Bcast(&Ex_EastSouth, 1, MPI_DOUBLE, rank_EastSouth, MPI_COMM_WORLD);
	MPI_Bcast(&Ey_EastSouth, 1, MPI_DOUBLE, rank_EastSouth, MPI_COMM_WORLD);

	//This correction is always done, independantly of the periodicity. Is this correct ?
	E_Add[0] = -0.5*(Ex_WestNorth+Ex_EastSouth);
	E_Add[1] = -0.5*(Ey_WestNorth+Ey_EastSouth);
    
    }
    else if( Ex_[0]->dims_.size()==1 ) {
	double Ex_West = 0.0;
	double Ex_East = 0.0;
    
	unsigned int rankWest = 0;
	if ( smpi->smilei_rk == 0 ) {
	    //Ex_West = (*Ex1D)(index_bc_min[0]);
	    Ex_West = (*this)( (0)-((*this).refHindex_))->EMfields->getEx_West();
	}
	MPI_Bcast(&Ex_West, 1, MPI_DOUBLE, rankWest, MPI_COMM_WORLD);
    
	unsigned int rankEast = smpi->smilei_sz-1;
	if ( smpi->smilei_rk == smpi->smilei_sz-1 ) {
	    //Ex_East = (*Ex1D)(index_bc_max[0]);
	    Ex_East = (*this)( (params.number_of_patches[0]-1)-((*this).refHindex_))->EMfields->getEx_East();
	}
	MPI_Bcast(&Ex_East, 1, MPI_DOUBLE, rankEast, MPI_COMM_WORLD);
	E_Add[0] = -0.5*(Ex_West+Ex_East);

    }

    // Centering electrostatic fields
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->EMfields->centeringE( E_Add );


    // Compute error on the Poisson equation
    double deltaPoisson_max = 0.0;
    int i_deltaPoisson_max  = -1;

#ifdef _A_FINALISER
    for (unsigned int i=0; i<nx_p; i++) {
        double deltaPoisson = abs( ((*Ex1D)(i+1)-(*Ex1D)(i))/dx - (*rho1D)(i) );
        if (deltaPoisson > deltaPoisson_max) {
            deltaPoisson_max   = deltaPoisson;
            i_deltaPoisson_max = i;
        }
    }
#endif
    
    //!\todo Reduce to find global max
    if (smpi->isMaster())
        MESSAGE(1,"Poisson equation solved. Maximum error = " << deltaPoisson_max << " at i= " << i_deltaPoisson_max);

}

bool VectorPatch::isRhoNull( SmileiMPI* smpi )
{
    double norm2(0.);
    double locnorm2(0.);
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	locnorm2 += (*this)(ipatch)->EMfields->computeRhoNorm2();

    MPI_Allreduce(&locnorm2, &norm2, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

    return (norm2<=0.);
}


void VectorPatch::exchange( std::vector<Field*> fields )
{
    unsigned int nx_, ny_, h0, oversize[2], n_space[2],gsp[2];
    double *pt1,*pt2;
    h0 = (*this)(0)->hindex;

    oversize[0] = (*this)(0)->EMfields->oversize[0];
    oversize[1] = (*this)(0)->EMfields->oversize[1];

    n_space[0] = (*this)(0)->EMfields->n_space[0];
    n_space[1] = (*this)(0)->EMfields->n_space[1];

    nx_ = fields[0]->dims_[0];
    ny_ = 1;
    if (fields[0]->dims_.size()>1)
      ny_ = fields[0]->dims_[1];

    gsp[0] = 2*oversize[0]+fields[0]->isDual_[0]; //Ghost size primal

    #pragma omp for schedule(dynamic) private(pt1,pt2)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {

	if ((*this)(ipatch)->MPI_me_ == (*this)(ipatch)->MPI_neighbor_[0][0]){
	    pt1 = &(*fields[(*this)(ipatch)->neighbor_[0][0]-h0])((n_space[0])*ny_);
	    pt2 = &(*fields[ipatch])(0);
	    memcpy( pt2, pt1, ny_*sizeof(double)); 
	    memcpy( pt1+gsp[0]*ny_, pt2+gsp[0]*ny_, ny_*sizeof(double)); 
	} // End if ( MPI_me_ == MPI_neighbor_[0][0] ) 

	if (fields[0]->dims_.size()>1) {
	    gsp[1] = 2*oversize[1]+fields[0]->isDual_[1]; //Ghost size primal
	    if ((*this)(ipatch)->MPI_me_ == (*this)(ipatch)->MPI_neighbor_[1][0]){
		pt1 = &(*fields[(*this)(ipatch)->neighbor_[1][0]-h0])(n_space[1]);
		pt2 = &(*fields[ipatch])(0);
		for (unsigned int i = 0 ; i < nx_*ny_ ; i += ny_){
		    pt2[i] = pt1[i] ;
		    pt1[i+gsp[1]] = pt2[i+gsp[1]] ;
		} 
	    } // End if ( MPI_me_ == MPI_neighbor_[1][0] ) 
	}

    } // End for( ipatch )

    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->initExchange( fields[ipatch], 0 );

    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->finalizeExchange( fields[ipatch], 0 );

    if (fields[0]->dims_.size()>1) {
        #pragma omp for
	for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	    (*this)(ipatch)->initExchange( fields[ipatch], 1 );

        #pragma omp for
	for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	    (*this)(ipatch)->finalizeExchange( fields[ipatch], 1 );
    }

}

void VectorPatch::exchange0( std::vector<Field*> fields )
{
    unsigned int nx_, ny_, h0, oversize[2], n_space[2],gsp[2];
    double *pt1,*pt2;
    h0 = (*this)(0)->hindex;

    oversize[0] = (*this)(0)->EMfields->oversize[0];
    oversize[1] = (*this)(0)->EMfields->oversize[1];

    n_space[0] = (*this)(0)->EMfields->n_space[0];
    n_space[1] = (*this)(0)->EMfields->n_space[1];

    nx_ = fields[0]->dims_[0];
    ny_ = 1;
    if (fields[0]->dims_.size()>1)
	ny_ = fields[0]->dims_[1];

    gsp[0] = 2*oversize[0]+fields[0]->isDual_[0]; //Ghost size primal

    #pragma omp for schedule(dynamic) private(pt1,pt2)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {

	if ((*this)(ipatch)->MPI_me_ == (*this)(ipatch)->MPI_neighbor_[0][0]){
	    pt1 = &(*fields[(*this)(ipatch)->neighbor_[0][0]-h0])((n_space[0])*ny_);
	    pt2 = &(*fields[ipatch])(0);
	    memcpy( pt2, pt1, ny_*sizeof(double)); 
	    memcpy( pt1+gsp[0]*ny_, pt2+gsp[0]*ny_, ny_*sizeof(double)); 
	} // End if ( MPI_me_ == MPI_neighbor_[0][0] ) 


    } // End for( ipatch )

    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->initExchange( fields[ipatch], 0 );

    #pragma omp for
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->finalizeExchange( fields[ipatch], 0 );


}

void VectorPatch::exchange1( std::vector<Field*> fields )
{
    unsigned int nx_, ny_, h0, oversize[2], n_space[2],gsp[2];
    double *pt1,*pt2;
    h0 = (*this)(0)->hindex;

    oversize[0] = (*this)(0)->EMfields->oversize[0];
    oversize[1] = (*this)(0)->EMfields->oversize[1];

    n_space[0] = (*this)(0)->EMfields->n_space[0];
    n_space[1] = (*this)(0)->EMfields->n_space[1];

    nx_ = fields[0]->dims_[0];
    ny_ = fields[0]->dims_[1];

    gsp[0] = 2*oversize[0]+fields[0]->isDual_[0]; //Ghost size primal
    gsp[1] = 2*oversize[1]+fields[0]->isDual_[1]; //Ghost size primal

    #pragma omp for schedule(runtime) private(pt1,pt2)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {

	if ((*this)(ipatch)->MPI_me_ == (*this)(ipatch)->MPI_neighbor_[1][0]){
	    pt1 = &(*fields[(*this)(ipatch)->neighbor_[1][0]-h0])(n_space[1]);
	    pt2 = &(*fields[ipatch])(0);
	    for (unsigned int i = 0 ; i < nx_*ny_ ; i += ny_){
		pt2[i] = pt1[i] ;
		pt1[i+gsp[1]] = pt2[i+gsp[1]] ;
	    } 
	} // End if ( MPI_me_ == MPI_neighbor_[1][0] ) 

    } // End for( ipatch )

    #pragma omp for schedule(runtime)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->initExchange( fields[ipatch], 1 );

    #pragma omp for schedule(runtime)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++)
	(*this)(ipatch)->finalizeExchange( fields[ipatch], 1 );


}

void VectorPatch::sum( std::vector<Field*> fields )
{
    unsigned int nx_,ny_, h0, oversize[2], n_space[2],gsp[2];
    double *pt1,*pt2;
    h0 = (*this)(0)->hindex;

    oversize[0] = (*this)(0)->EMfields->oversize[0];
    oversize[1] = (*this)(0)->EMfields->oversize[1];
    
    n_space[0] = (*this)(0)->EMfields->n_space[0];
    n_space[1] = (*this)(0)->EMfields->n_space[1];
    
    nx_ = fields[0]->dims_[0];
    ny_ = 1;
    if (fields[0]->dims_.size()>1)
        ny_ = fields[0]->dims_[1];
    
    gsp[0] = 1+2*oversize[0]+fields[0]->isDual_[0]; //Ghost size primal


    #pragma omp for schedule(runtime) private(pt1,pt2)
    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {

        if ((*this)(ipatch)->MPI_me_ == (*this)(ipatch)->MPI_neighbor_[0][0]){
	    //The patch on my left belongs to the same MPI process than I.
	    pt1 = &(*fields[(*this)(ipatch)->neighbor_[0][0]-h0])(n_space[0]*ny_);
	    pt2 = &(*fields[ipatch])(0);
	    for (unsigned int i = 0; i < gsp[0]* ny_ ; i++) pt1[i] += pt2[i];
	    memcpy( pt2, pt1, gsp[0]*ny_*sizeof(double)); 
                    
	}

    }
    
    for (int iDim=0;iDim<1;iDim++) {
        #pragma omp for schedule(runtime)
        for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
    	(*this)(ipatch)->initSumField( fields[ipatch], iDim ); // initialize
        }
    
        #pragma omp for schedule(runtime)
        for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
    	(*this)(ipatch)->finalizeSumField( fields[ipatch], iDim ); // finalize (waitall + sum)
        }
    }


    if (fields[0]->dims_.size()>1) {
	gsp[1] = 1+2*oversize[1]+fields[0]->isDual_[1]; //Ghost size primal
        #pragma omp for schedule(runtime) private(pt1,pt2)
	for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {

	    if ((*this)(ipatch)->MPI_me_ == (*this)(ipatch)->MPI_neighbor_[1][0]){
		//The patch below me belongs to the same MPI process than I.
		pt1 = &(*fields[(*this)(ipatch)->neighbor_[1][0]-h0])(n_space[1]);
		pt2 = &(*fields[ipatch])(0);
		for (unsigned int j = 0; j < nx_ ; j++){
		    for (unsigned int i = 0; i < gsp[1] ; i++) pt1[i] += pt2[i];
		    memcpy( pt2, pt1, gsp[1]*sizeof(double)); 
		    pt1 += ny_;
		    pt2 += ny_;
		}
	    }
	}

	for (int iDim=1;iDim<2;iDim++) {
            #pragma omp for schedule(runtime)
	    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	        (*this)(ipatch)->initSumField( fields[ipatch], iDim ); // initialize
	    }

            #pragma omp for schedule(runtime)
	    for (unsigned int ipatch=0 ; ipatch<this->size() ; ipatch++) {
	        (*this)(ipatch)->finalizeSumField( fields[ipatch], iDim ); // finalize (waitall + sum)
	    }
	}
    }
}

void VectorPatch::resizeFields()
{
        Jx_.resize( size());
        Jy_.resize( size());
        Jz_.resize( size());
        rho_.resize(size());
        Ex_.resize( size());
        Ey_.resize( size());
        Ez_.resize( size());
        Bx_.resize( size());
        By_.resize( size());
        Bz_.resize( size());
}
