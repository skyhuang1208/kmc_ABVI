#include <cstdio>
#include <iostream>
#include <vector>
#include "kmc_global.h"
#include "kmc_events.h"

using namespace std;

double class_events::jump(){
	// a probability map will first generated by calculating all possible moves
	// then randomly picked the ACTUAL move based on the probability map

	// defect information
	vector <bool>   isvcc; // a vacancy of interstitial?
	vector <double> rates; // transition rates
	vector <int>    ilist; // IDs in the lists
	vector <int>    inbrs; // IDs of neighbor directions
	vector <int>	jatom; // the jumping atom
	
	// perform imaginary jumps and cal rates
	double vrates= cal_ratesV(isvcc, rates, ilist, inbrs, jatom);
	double irates= cal_ratesI(isvcc, rates, ilist, inbrs, jatom);

	double sum_rates= vrates + irates + rate_genr; // sum of all rates
	double ran= ran_generator();
	double acc_rate= 0; // accumulated rate

	for(int i=0; i<rates.size(); i ++){
		if( (ran >= acc_rate) && (ran < (acc_rate + rates[i]/sum_rates) ) ){			
			if(isvcc[i]){
				if(jatom[i]==1) Vja[0] ++;
				else		Vja[1] ++;

				actual_jumpV(ilist[i], inbrs[i]);
			
				int xv= (int) (list_vcc[ilist[i]].ltcp/nz)/ny;
				if(xv==x_sink)   sink(true, ilist[i]);
				else           recb_randomV(ilist[i]);
			}
			else{
				if(jatom[i]==1) Ija[0] ++;
				else		Ija[1] ++;

				actual_jumpI(ilist[i], inbrs[i], jatom[i]);
				
				int xi= (int) (list_itl[ilist[i]].ltcp/nz)/ny;
				if(xi==x_sink) sink(false, ilist[i]);
				else          recb_randomI(ilist[i]);
			
			}
			
			goto skip_genr;
		}
		
		acc_rate += rates[i]/sum_rates;
	}
	
	genr(); // Frenkel pair genr
    N_genr ++;

skip_genr:;

	if(nA+nB+nV+nAA+nBB+nAB != nx*ny*nz)  error(2, "(jump) numbers of ltc points arent consistent, diff=", 1, nA+nB+nV+nAA+nBB+nAB-nx*ny*nz); // check
//	if(2*nAA+nA-nB-2*nBB != sum_mag)  error(2, "(jump) magnitization isnt conserved", 2, 2*nAA+nA-nB-2*nBB, sum_mag); // dont use it since reservior

	return 1.0/sum_rates;
}

void class_events::actual_jumpV(int vid, int nid){ // vcc id and neighbor id
	int xv= (int) (list_vcc[vid].ltcp/nz)/ny; // vcc position
	int yv= (int) (list_vcc[vid].ltcp/nz)%ny;
	int zv= (int)  list_vcc[vid].ltcp%nz;
			
	int x= pbc(xv+v1nbr[nid][0], nx); // neighbor position
	int y= pbc(yv+v1nbr[nid][1], ny);
	int z= pbc(zv+v1nbr[nid][2], nz);
	
	states[xv][yv][zv]= states[x][y][z];
	states[x][y][z]= 0;
	list_vcc[vid].ltcp= x*ny*nz + y*nz + z;
				
	if((x-xv)>nx/2) list_vcc[vid].ix --; if((x-xv)<-nx/2) list_vcc[vid].ix ++;
	if((y-yv)>ny/2) list_vcc[vid].iy --; if((y-yv)<-ny/2) list_vcc[vid].iy ++;
	if((z-zv)>nz/2) list_vcc[vid].iz --; if((z-zv)<-nz/2) list_vcc[vid].iz ++;

	// sol hash?
}

void class_events::actual_jumpI(int iid, int nid, int jatom){ // itl id, neighbor id, and the jumping atom
	int xi= (int) (list_itl[iid].ltcp/nz)/ny; // itl position
	int yi= (int) (list_itl[iid].ltcp/nz)%ny;
	int zi= (int)  list_itl[iid].ltcp%nz;
			
	int x= pbc(xi+v1nbr[nid][0], nx); // neighbor position
	int y= pbc(yi+v1nbr[nid][1], ny);
	int z= pbc(zi+v1nbr[nid][2], nz);
	
	switch(states[xi][yi][zi]){ // update numbers before jump
		case  2: nAA --; break;
		case  0: nAB --; itlAB[xi][yi][zi]= false; break;
		case -2: nBB --; break;
		default: error(2, "(jump) could not find the Itl type in --", 1, states[xi][yi][zi]);
	}
	switch(states[x][y][z]){
		case  1: nA --; 
			 break;
		case -1: nB --;
			 break;
			 // sol hash?
		default: error(2, "(jump) could not find the Atom type in --", 1, states[x][y][z]);
	}

	states[xi][yi][zi] -= jatom; // jumping
	states[x][y][z]    += jatom;
	
	switch(states[x][y][z]){ // update numbers after jump
		case  2: nAA ++; break;
		case  0: nAB ++; itlAB[x][y][z]= true; break;
		case -2: nBB ++; break;
		default: error(2, "(jump) could not find the Itl type in ++", 1, states[x][y][z]);
	}
	switch(states[xi][yi][zi]){
		case  1: nA ++; break;
		case -1: nB ++; break;
			 // sol hash?
		default: error(2, "(jump) could not find the Atom type in ++", 1, states[xi][yi][zi]);
	}
	
	list_itl[iid].ltcp= x*ny*nz + y*nz + z;
	list_itl[iid].dir=  nid;
	list_itl[iid].head= states[x][y][z] - jatom;
	
	if((x-xi)>nx/2) list_itl[iid].ix --; if((x-xi)<-nx/2) list_itl[iid].ix ++;
	if((y-yi)>ny/2) list_itl[iid].iy --; if((y-yi)<-ny/2) list_itl[iid].iy ++;
	if((z-zi)>nz/2) list_itl[iid].iz --; if((z-zi)<-nz/2) list_itl[iid].iz ++;
}
	
// functions in backupfun:
//	int vpos[3];
//	events.vac_jump_random(par_pr_vjump, vpos);
//	events.vac_recb(vpos);
//	events.int_motions();

