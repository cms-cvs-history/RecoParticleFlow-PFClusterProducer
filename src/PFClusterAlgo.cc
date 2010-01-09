#include "RecoParticleFlow/PFClusterProducer/interface/PFClusterAlgo.h"
#include "DataFormats/ParticleFlowReco/interface/PFLayer.h"
#include "DataFormats/HcalDetId/interface/HcalDetId.h"
#include "Math/GenVector/VectorUtil.h"


#include <stdexcept>
#include <string>

using namespace std;

unsigned PFClusterAlgo::prodNum_ = 1;

//for debug only 
//#define PFLOW_DEBUG

PFClusterAlgo::PFClusterAlgo() :
  pfClusters_( new vector<reco::PFCluster> ),
  threshBarrel_(0.),
  threshPtBarrel_(0.),
  threshSeedBarrel_(0.2),
  threshPtSeedBarrel_(0.),
  threshEndcap_(0.),
  threshPtEndcap_(0.),
  threshSeedEndcap_(0.6),
  threshPtSeedEndcap_(0.),
  threshCleanBarrel_(1E5),
  minS4S1Barrel_(0.),
  threshCleanEndcap_(1E5),
  minS4S1Endcap_(0.),
  nNeighbours_(4),
  posCalcNCrystal_(-1),
  posCalcP1_(-1),
  showerSigma_(5),
  useCornerCells_(false),
  debug_(false) {}



void PFClusterAlgo::doClustering( const PFRecHitHandle& rechitsHandle ) {
  rechitsHandle_ = rechitsHandle;
  doClustering( *rechitsHandle );
}

void PFClusterAlgo::doClustering( const reco::PFRecHitCollection& rechits ) {


  if(pfClusters_.get() ) pfClusters_->clear();
  else 
    pfClusters_.reset( new std::vector<reco::PFCluster> );


  eRecHits_.clear();

  bool initMask = false;
  if( mask_.size() != rechits.size() ) {
    initMask = true;
    mask_.clear();
    mask_.reserve( rechits.size() );

    if( ! mask_.empty() ) 
      cerr<<"PClusterAlgo::doClustering: map size should be "<<mask_.size()
	  <<". Will be reinitialized."<<endl;    
  }
  
  color_.clear(); 
  color_.reserve( rechits.size() );
  seedStates_.clear();
  seedStates_.reserve( rechits.size() );
  usedInTopo_.clear();
  usedInTopo_.reserve( rechits.size() );
  
  for ( unsigned i = 0; i < rechits.size(); i++ ) {
    eRecHits_.insert( make_pair( rechit(i, rechits).energy(), i) );
    if(initMask) mask_.push_back( true );
    color_.push_back( 0 );     
    seedStates_.push_back( UNKNOWN ); 
    usedInTopo_.push_back( false ); 
  }  

  cleanRBXAndHPD( rechits);

  // look for seeds.
  findSeeds( rechits );

  // build topological clusters around seeds
  buildTopoClusters( rechits );
  
  // look for PFClusters inside each topological cluster (one per seed)
  for(unsigned i=0; i<topoClusters_.size(); i++) {

    const std::vector< unsigned >& topocluster = topoClusters_[i];
    buildPFClusters( topocluster, rechits ); 
  }
}


void PFClusterAlgo::setMask( const std::vector<bool>& mask ) {
  mask_ = mask;
}




double PFClusterAlgo::parameter( Parameter paramtype, 
				 PFLayer::Layer layer)  const {
  

  double value = 0;

  switch( layer ) {
  case PFLayer::ECAL_BARREL:
  case PFLayer::HCAL_BARREL1:
  case PFLayer::HCAL_BARREL2: // I think this is HO. 
                              // should not do anything for HO !
    switch(paramtype) {
    case THRESH:
      value = threshBarrel_;
      break;
    case SEED_THRESH:
      value = threshSeedBarrel_;
      break;
    case PT_THRESH:
      value = threshPtBarrel_;
      break;
    case SEED_PT_THRESH:
      value = threshPtSeedBarrel_;
      break;
    case CLEAN_THRESH:
      value = threshCleanBarrel_;
      break;
    case CLEAN_S4S1:
      value = minS4S1Barrel_;
      break;
    default:
      cerr<<"PFClusterAlgo::parameter : unknown parameter type "
	  <<paramtype<<endl;
      assert(0);
    }
    break;
  case PFLayer::ECAL_ENDCAP:
  case PFLayer::HCAL_ENDCAP:
  case PFLayer::PS1:
  case PFLayer::PS2:
  case PFLayer::HF_EM:
  case PFLayer::HF_HAD:
    // and no particle flow in VFCAL
    switch(paramtype) {
    case THRESH:
      value = threshEndcap_;
      break;
    case SEED_THRESH:
      value = threshSeedEndcap_;
      break;
    case PT_THRESH:
      value = threshPtEndcap_;
      break;
    case SEED_PT_THRESH:
      value = threshPtSeedEndcap_;
      break;
    case CLEAN_THRESH:
      value = threshCleanEndcap_;
      break;
    case CLEAN_S4S1:
      value = minS4S1Endcap_;
      break;
    default:
      cerr<<"PFClusterAlgo::parameter : unknown parameter type "
	  <<paramtype<<endl;
      assert(0);
    }
    break;
  default:
    cerr<<"PFClusterAlgo::parameter : unknown layer "<<layer<<endl;
    assert(0);
    break;
  }

  return value;
}


void 
PFClusterAlgo::cleanRBXAndHPD(  const reco::PFRecHitCollection& rechits ) {

  std::map< int, std::vector<unsigned> > hpds;
  std::map< int, std::vector<unsigned> > rbxs;

  for(EH ih = eRecHits_.begin(); ih != eRecHits_.end(); ih++ ) {

    unsigned  rhi      = ih->second; 

    if(! masked(rhi) ) continue;
    // rechit was asked to be processed
    const reco::PFRecHit& rhit = rechit( rhi, rechits);
    //double energy = rhit.energy();
    int layer = rhit.layer();
    if ( layer != PFLayer::HCAL_BARREL1 &&
	 layer != PFLayer::HCAL_ENDCAP ) break;
    HcalDetId theHcalDetId = HcalDetId(rhit.detId());
    int ieta = theHcalDetId.ieta();
    int iphi = theHcalDetId.iphi();
    int ihpd = ieta < 0 ?  
      ( layer == PFLayer::HCAL_ENDCAP ?  -(iphi+1)/2-100 : -iphi ) : 
      ( layer == PFLayer::HCAL_ENDCAP ?   (iphi+1)/2+100 :  iphi ) ;      
    hpds[ihpd].push_back(rhi);
    int irbx = ieta < 0 ? 
      ( layer == PFLayer::HCAL_ENDCAP ?  -(iphi+5)/4 - 20 : -(iphi+5)/4 ) : 
      ( layer == PFLayer::HCAL_ENDCAP ?   (iphi+5)/4 + 20 :  (iphi+5)/4 ) ;      
    if ( irbx == 19 ) irbx = 1;
    else if ( irbx == -19 ) irbx = -1;
    else if ( irbx == 39 ) irbx = 21;
    else if ( irbx == -39 ) irbx = -21;
    rbxs[irbx].push_back(rhi);
  }

  // Loop on readout boxes
  for ( std::map<int, std::vector<unsigned> >::iterator itrbx = rbxs.begin();
	itrbx != rbxs.end(); ++itrbx ) { 
    if ( ( abs(itrbx->first)<20 && itrbx->second.size() > 30 ) || 
	 ( abs(itrbx->first)>20 && itrbx->second.size() > 30 ) ) { 
      const std::vector<unsigned>& rhits = itrbx->second;
      double totalEta = 0.;
      double totalEtaW = 0.;
      double totalPhi = 0.;
      double totalPhiW = 0.;
      double totalEta2 = 1E-9;
      double totalEta2W = 1E-9;
      double totalPhi2 = 1E-9;
      double totalPhi2W = 1E-9;
      double totalEnergy = 0.;
      double totalEnergy2 = 1E-9;
      unsigned nSeeds = rhits.size();
      unsigned nSeeds0 = rhits.size();
      std::map< int,std::vector<unsigned> > theHPDs;
      for ( unsigned jh=0; jh < rhits.size(); ++jh ) {
	const reco::PFRecHit& hit = rechit(rhits[jh], rechits);
	// Check if the hit is a seed
	unsigned nN = 0;
	bool isASeed = true;
	const vector<unsigned>& neighbours4 = *(& hit.neighbours4());
	for(unsigned in=0; in<neighbours4.size(); in++) {
	  const reco::PFRecHit& neighbour = rechit( neighbours4[in], rechits ); 
	  // one neighbour has a higher energy -> the tested rechit is not a seed
	  if( neighbour.energy() > hit.energy() ) {
	    --nSeeds;
	    --nSeeds0;
	    isASeed = false;
	    break;
	  } else {
	    if ( neighbour.energy() > 0.4 ) ++nN;
	  }
	}
	if ( isASeed && !nN ) --nSeeds0;

	HcalDetId theHcalDetId = HcalDetId(hit.detId());
	// int ieta = theHcalDetId.ieta();
	int iphi = theHcalDetId.iphi();
	// std::cout << "Hit : " << hit.energy() << " " << ieta << " " << iphi << std::endl;
	if ( hit.layer() == PFLayer::HCAL_BARREL1 )
	  theHPDs[iphi].push_back(rhits[jh]);
	else
	  theHPDs[(iphi-1)/2].push_back(rhits[jh]);
	totalEnergy += hit.energy();
	totalPhi += fabs(hit.position().phi());
	totalPhiW += hit.energy()*fabs(hit.position().phi());
	totalEta += hit.position().eta();
	totalEtaW += hit.energy()*hit.position().eta();
	totalEnergy2 += hit.energy()*hit.energy();
	totalPhi2 += hit.position().phi()*hit.position().phi();
	totalPhi2W += hit.energy()*hit.position().phi()*hit.position().phi();
	totalEta2 += hit.position().eta()*hit.position().eta();
	totalEta2W += hit.energy()*hit.position().eta()*hit.position().eta();
      }
      // totalPhi /= totalEnergy;
      totalPhi /= rhits.size();
      totalEta /= rhits.size();
      totalPhiW /= totalEnergy;
      totalEtaW /= totalEnergy;
      totalPhi2 /= rhits.size();
      totalEta2 /= rhits.size();
      totalPhi2W /= totalEnergy;
      totalEta2W /= totalEnergy;
      totalPhi2 = std::sqrt(totalPhi2 - totalPhi*totalPhi);
      totalEta2 = std::sqrt(totalEta2 - totalEta*totalEta);
      totalPhi2W = std::sqrt(totalPhi2W - totalPhiW*totalPhiW);
      totalEta2W = std::sqrt(totalEta2W - totalEtaW*totalEtaW);
      totalEnergy /= rhits.size();
      totalEnergy2 /= rhits.size();
      totalEnergy2 = std::sqrt(totalEnergy2 - totalEnergy*totalEnergy);
      //if ( totalPhi2W/totalEta2W < 0.18 ) { 
      if ( nSeeds0 > 6 ) {
	unsigned nHPD15 = 0;
	for ( std::map<int, std::vector<unsigned> >::iterator itHPD = theHPDs.begin();
	      itHPD != theHPDs.end(); ++itHPD ) { 
	  int hpdN = itHPD->first;
	  const std::vector<unsigned>& hpdHits = itHPD->second;
	  if ( ( abs(hpdN) < 100 && hpdHits.size() > 14 ) || 
	       ( abs(hpdN) > 100 && hpdHits.size() > 14 ) ) ++nHPD15;
	}
	if ( nHPD15 > 1 ) { 
	  std::cout << "Read out box numero " << itrbx->first 
		    << " has " << itrbx->second.size() << " hits in it !"
		    << std::endl << "sigma Eta/Phi = " << totalEta2 << " " << totalPhi2 << " " << totalPhi2/totalEta2
		    << std::endl << "sigma EtaW/PhiW = " << totalEta2W << " " << totalPhi2W << " " << totalPhi2W/totalEta2W
		    << std::endl << "E = " << totalEnergy << " +/- " << totalEnergy2
		    << std::endl << "nSeeds = " << nSeeds << " " << nSeeds0
		    << std::endl;
	  for ( std::map<int, std::vector<unsigned> >::iterator itHPD = theHPDs.begin();
		itHPD != theHPDs.end(); ++itHPD ) { 
	    unsigned hpdN = itHPD->first;
	    const std::vector<unsigned>& hpdHits = itHPD->second;
	    std::cout << "HPD number " << hpdN << " contains " << hpdHits.size() << " hits" << std::endl;
	  }
	}
      }
    }
  }

  // Loop on hpd's
  std::map<int, std::vector<unsigned> >::iterator neighbour1;
  std::map<int, std::vector<unsigned> >::iterator neighbour2;
  unsigned size1 = 0;
  unsigned size2 = 0;
  for ( std::map<int, std::vector<unsigned> >::iterator ithpd = hpds.begin();
	ithpd != hpds.end(); ++ithpd ) { 

    const std::vector<unsigned>& rhits = ithpd->second;
    double totalEnergy = 0.;
    double totalEnergy2 = 1E-9;
    for ( unsigned jh=0; jh < rhits.size(); ++jh ) {
      const reco::PFRecHit& hit = rechit(rhits[jh], rechits);
      totalEnergy += hit.energy();
      totalEnergy2 += hit.energy()*hit.energy();
    }
    totalEnergy /= rhits.size();
    totalEnergy2 /= rhits.size();
    totalEnergy2 = std::sqrt(totalEnergy2 - totalEnergy*totalEnergy);

    if ( ithpd->first == 1 ) neighbour1 = hpds.find(72);
    else if ( ithpd->first == -1 ) neighbour1 = hpds.find(-72);
    else if ( ithpd->first == 101 ) neighbour1 = hpds.find(136);
    else if ( ithpd->first == -101 ) neighbour1 = hpds.find(-136);
    else neighbour1 = ithpd->first > 0 ? hpds.find(ithpd->first-1) : hpds.find(ithpd->first+1) ;

    if ( ithpd->first == 72 ) neighbour2 = hpds.find(1);
    else if ( ithpd->first == -72 ) neighbour2 = hpds.find(-1);
    else if ( ithpd->first == 136 ) neighbour2 = hpds.find(101);
    else if ( ithpd->first == -136 ) neighbour2 = hpds.find(-101);
    else neighbour2 = ithpd->first > 0 ? hpds.find(ithpd->first+1) : hpds.find(ithpd->first-1) ;
    
    size1 = neighbour1 != hpds.end() ? neighbour1->second.size() : 0;
    size2 = neighbour2 != hpds.end() ? neighbour2->second.size() : 0;
    
    //if ( ( abs(ithpd->first) > 100 && ithpd->second.size() > 13 ) || 
    //     ( abs(ithpd->first) < 100 && ithpd->second.size() > 11 ) )
    //  if ( (float)(size1 + size2)/(float)ithpd->second.size() < 0.5 ) 
    if ( ( abs(ithpd->first) > 100 && ithpd->second.size() > 15 ) || 
         ( abs(ithpd->first) < 100 && ithpd->second.size() > 12 ) )
      if ( (float)(size1 + size2)/(float)ithpd->second.size() < 1.0 ) 
	std::cout << "HPD numero " << ithpd->first 
		  << " has " << ithpd->second.size() << " hits in it !" << std::endl
		  << "Neighbours : " << size1 << " " << size2
		  << std::endl;

  }

}


void PFClusterAlgo::findSeeds( const reco::PFRecHitCollection& rechits ) {

  seeds_.clear();

  // should replace this by the message logger.
#ifdef PFLOW_DEBUG
  if(debug_) 
    cout<<"PFClusterAlgo::findSeeds : start"<<endl;
#endif


  // An empty list of neighbours
  const vector<unsigned> noNeighbours(0, static_cast<unsigned>(0));

  // loop on rechits (sorted by decreasing energy - not E_T)
  for(EH ih = eRecHits_.begin(); ih != eRecHits_.end(); ih++ ) {

    unsigned  rhi      = ih->second; 

    if(! masked(rhi) ) continue;
    // rechit was asked to be processed

    double    rhenergy = ih->first;   
    const reco::PFRecHit& wannaBeSeed = rechit(rhi, rechits);
     
    if( seedStates_[rhi] == NO ) continue;
    // this hit was already tested, and is not a seed
 
    // determine seed energy threshold depending on the detector
    int layer = wannaBeSeed.layer();
    double seedThresh = parameter( SEED_THRESH, 
				   static_cast<PFLayer::Layer>(layer) );
    double seedPtThresh = parameter( SEED_PT_THRESH, 
				     static_cast<PFLayer::Layer>(layer) );
    double cleanThresh = parameter( CLEAN_THRESH, 
				    static_cast<PFLayer::Layer>(layer) );
    double minS4S1 = parameter( CLEAN_S4S1, 
				static_cast<PFLayer::Layer>(layer) );


#ifdef PFLOW_DEBUG
    if(debug_) 
      cout<<"layer:"<<layer<<" seedThresh:"<<seedThresh<<endl;
#endif


    if( rhenergy < seedThresh || (seedPtThresh>0. && wannaBeSeed.pt2() < seedPtThresh*seedPtThresh )) {
      seedStates_[rhi] = NO; 
      continue;
    } 

      
    // Find the cell unused neighbours
    const vector<unsigned>* nbp;
    double tighter = 1.0;

    switch ( layer ) { 
    case PFLayer::ECAL_BARREL:         
    case PFLayer::ECAL_ENDCAP:       
    case PFLayer::HCAL_BARREL1:
    case PFLayer::HCAL_BARREL2:
    case PFLayer::HCAL_ENDCAP:
      tighter = 2.0;
    case PFLayer::HF_EM:
    case PFLayer::HF_HAD:
      if( nNeighbours_ == 4 ) {
	nbp = & wannaBeSeed.neighbours4();
      }
      else if( nNeighbours_ == 8 ) {
	nbp = & wannaBeSeed.neighbours8();
      }
      else if( nNeighbours_ == 0 ) {
	nbp = & noNeighbours;
	// Allows for no clustering at all: all rechits are clusters.
	// Useful for HF
      }
      else {
	cerr<<"you're not allowed to set n neighbours to "
	    <<nNeighbours_<<endl;
	assert(0);
      }
      break;
    case PFLayer::PS1:       
    case PFLayer::PS2:     
      nbp = & wannaBeSeed.neighbours4();
      break;

    default:
      cerr<<"CellsEF::PhotonSeeds : unknown layer "<<layer<<endl;
      assert(0);
    }

    const vector<unsigned>& neighbours = *nbp;

      
    // Select as a seed if all neighbours have a smaller energy

    seedStates_[rhi] = YES;
    for(unsigned in=0; in<neighbours.size(); in++) {
	
      const reco::PFRecHit& neighbour = rechit( neighbours[in], 
						rechits ); 
	
      // one neighbour has a higher energy -> the tested rechit is not a seed
      if( neighbour.energy() > wannaBeSeed.energy() ) {
	seedStates_[rhi] = NO;
	break;
      }
    }
      

    // Cleaning : check energetic, isolated seeds, likely to come from erratic noise.
    if ( wannaBeSeed.energy() > cleanThresh ) { 
      const vector<unsigned>& neighbours4 = *(& wannaBeSeed.neighbours4());
      // Determine the fraction of surrounding energy
      //std::cout << "Energy of the cell and energy Up : " << wannaBeSeed.energy() 
      //          << " " << wannaBeSeed.energyUp() << std::endl;
      //std::cout << "Energies of the neighbours : " << std::endl;
      double surroundingEnergy = wannaBeSeed.energyUp();
      double neighbourEnergy = 0.;
      double layerEnergy = 0.;
      for(unsigned in4=0; in4<neighbours4.size(); in4++) {
	const reco::PFRecHit& neighbour = rechit( neighbours4[in4], rechits ); 
	surroundingEnergy += neighbour.energy() + neighbour.energyUp();
	neighbourEnergy += neighbour.energy() + neighbour.energyUp();
	layerEnergy += neighbour.energy();
	//std::cout << in4 << ": " << neighbour.energy() << " " << neighbour.energyUp() << std::endl;
      }
      // Fraction 0 is the balance between EM and HAD layer for this tower
      double fraction0 = layer == PFLayer::HF_EM || layer == PFLayer::HF_HAD ? 
	wannaBeSeed.energyUp()/wannaBeSeed.energy() : 1.;
      // Fraction 1 is the balance between the hit and its neighbours from both layers
      double fraction1 = surroundingEnergy/wannaBeSeed.energy();
      // Fraction 2 is the balance between the tower and the tower neighbours
      double fraction2 = neighbourEnergy/(wannaBeSeed.energy()+wannaBeSeed.energyUp());
      // Fraction 3 is the balance between the hits and the hits neighbours in the same layer.
      double fraction3 = layerEnergy/(wannaBeSeed.energy());
      // Mask the seed and the hit if energetic/isolated rechit
      // if ( fraction0 < minS4S1 || fraction1 < minS4S1 || fraction2 < minS4S1 || fraction3 < minS4S1 ) {
      // if ( fraction1 < minS4S1 || ( wannaBeSeed.energy() > 1.5*cleanThresh && fraction0 + fraction3 < minS4S1 ) ) {
      if ( fraction1 < minS4S1 ) {
	// Double the energy cleaning threshold when close to the ECAL/HCAL - HF transition
	double eta = fabs(wannaBeSeed.position().eta());
	if (   eta < 5.0 &&                                            // No cleaning for the HF border 
	     ( eta < 2.8 || rhenergy > tighter*cleanThresh ) &&        // Tighter cleaning for the EB-HB/HF transition 
	     ( abs(eta-1.48) > 0.02 || fraction1 < minS4S1/tighter )   // Tighter cleaning for the EB/EE transition 
	    ) { 
	  seedStates_[rhi] = CLEAN;
	  mask_[rhi] = false;
	  std::cout << "A seed with E/pT/eta = " << wannaBeSeed.energy() << " " << wannaBeSeed.energyUp() 
		    << " " << sqrt(wannaBeSeed.pt2()) << " " << wannaBeSeed.position().eta() 
		    << " and with surrounding fractions = " << fraction0 << " " << fraction1 
		    << " " << fraction2 << " " << fraction3
		    << " in layer " << layer 
		    << " had been cleaned " << std::endl
		    << "(Cuts were : " << cleanThresh << " and " << minS4S1 << std::endl; 
	}
      }
    }

    if ( seedStates_[rhi] == YES ) {

      // seeds_ contains the indices of all seeds. 
      seeds_.push_back( rhi );
      
      // marking the rechit
      paint(rhi, SEED);
	
      // then all neighbours cannot be seeds and are flagged as such
      for(unsigned in=0; in<neighbours.size(); in++) {
	seedStates_[ neighbours[in] ] = NO;
      }
    }
  }  

#ifdef PFLOW_DEBUG
  if(debug_) 
    cout<<"PFClusterAlgo::findSeeds : done"<<endl;
#endif
}



  
void PFClusterAlgo::buildTopoClusters( const reco::PFRecHitCollection& rechits ){

  topoClusters_.clear(); 
  
#ifdef PFLOW_DEBUG
  if(debug_) 
    cout<<"PFClusterAlgo::buildTopoClusters start"<<endl;
#endif
  
  for(unsigned is = 0; is<seeds_.size(); is++) {
    
    unsigned rhi = seeds_[is];

    if( !masked(rhi) ) continue;
    // rechit was masked to be processed

    // already used in a topological cluster
    if( usedInTopo_[rhi] ) {
#ifdef PFLOW_DEBUG
      if(debug_) 
	cout<<rhi<<" used"<<endl; 
#endif
      continue;
    }
    
    vector< unsigned > topocluster;
    buildTopoCluster( topocluster, rhi, rechits );
   
    if(topocluster.empty() ) continue;
    
    topoClusters_.push_back( topocluster );

  }

#ifdef PFLOW_DEBUG
  if(debug_) 
    cout<<"PFClusterAlgo::buildTopoClusters done"<<endl;
#endif
  
  return;
}


void 
PFClusterAlgo::buildTopoCluster( vector< unsigned >& cluster,
				 unsigned rhi, 
				 const reco::PFRecHitCollection& rechits ){


#ifdef PFLOW_DEBUG
  if(debug_)
    cout<<"PFClusterAlgo::buildTopoCluster in"<<endl;
#endif

  const reco::PFRecHit& rh = rechit( rhi, rechits); 

  double e = rh.energy();
  int layer = rh.layer();
  
  double thresh = parameter( THRESH, 
			     static_cast<PFLayer::Layer>(layer) );
  double ptThresh = parameter( PT_THRESH, 
			       static_cast<PFLayer::Layer>(layer) );


  if( e < thresh ||  (ptThresh > 0. && rh.pt2() < ptThresh*ptThresh) ) {
#ifdef PFLOW_DEBUG
    if(debug_)
      cout<<"return : "<<e<<"<"<<thresh<<endl; 
#endif
    return;
  }

  // add hit to cluster

  cluster.push_back( rhi );
  // idUsedRecHits_.insert( rh.detId() );

  usedInTopo_[ rhi ] = true;

  //   cout<<" hit ptr "<<hit<<endl;

  // get neighbours, either with one side in common, 
  // or with one corner in common (if useCornerCells_)
  const std::vector< unsigned >& nbs 
    = useCornerCells_ ? rh.neighbours8() : rh.neighbours4();
  
  for(unsigned i=0; i<nbs.size(); i++) {

//     const reco::PFRecHit& neighbour = rechit( nbs[i], rechits );

//     set<unsigned>::iterator used 
//       = idUsedRecHits_.find( neighbour.detId() );
//     if(used != idUsedRecHits_.end() ) continue;
    
    // already used
    if( usedInTopo_[ nbs[i] ] ) {
#ifdef PFLOW_DEBUG
      if(debug_) 
	cout<<rhi<<" used"<<endl; 
#endif
      continue;
    }
			     
    buildTopoCluster( cluster, nbs[i], rechits );
  }
#ifdef PFLOW_DEBUG
  if(debug_)
    cout<<"PFClusterAlgo::buildTopoCluster out"<<endl;
#endif

}


void 
PFClusterAlgo::buildPFClusters( const std::vector< unsigned >& topocluster,
				const reco::PFRecHitCollection& rechits ) 
{


  //  bool debug = false;


  // several rechits may be seeds. initialize PFClusters on these seeds. 
  
  vector<reco::PFCluster> curpfclusters;
  vector<reco::PFCluster> curpfclusterswodepthcor;
  vector< unsigned > seedsintopocluster;


  for(unsigned i=0; i<topocluster.size(); i++ ) {

    unsigned rhi = topocluster[i];

    if( seedStates_[rhi] == YES ) {

      reco::PFCluster cluster;
      reco::PFCluster clusterwodepthcor;

      double fraction = 1.0; 
      
      reco::PFRecHitRef  recHitRef = createRecHitRef( rechits, rhi ); 
	
      cluster.addRecHitFraction( reco::PFRecHitFraction( recHitRef, 
							 fraction ) );

    // cluster.addRecHit( rhi, fraction );
      
      calculateClusterPosition( cluster,
                                clusterwodepthcor, 
			        true );    


//       cout<<"PFClusterAlgo: 2"<<endl;
      curpfclusters.push_back( cluster );
      curpfclusterswodepthcor.push_back( clusterwodepthcor );
#ifdef PFLOW_DEBUG
      if(debug_) {
	cout << "PFClusterAlgo::buildPFClusters: seed "
	     << rechit( rhi, rechits) <<endl;
	cout << "PFClusterAlgo::buildPFClusters: pfcluster initialized : "
	     << cluster <<endl;
      }
#endif

      // keep track of the seed of each topocluster
      seedsintopocluster.push_back( rhi );
      
    }
  }

  // if only one seed in the topocluster, use all crystals
  // in the position calculation (posCalcNCrystal = -1)
  // otherwise, use the user specified value
  int posCalcNCrystal = seedsintopocluster.size()>1 ? posCalcNCrystal_:-1;
  double ns2 = std::max(1.,(double)(seedsintopocluster.size())-1.);
  ns2 *= ns2;
    
  // Find iteratively the energy and position
  // of each pfcluster in the topological cluster
  unsigned iter = 0;
  unsigned niter = 50;
  double diff = ns2;

  // if(debug_) niter=2;
  vector<double> ener;
  vector<double> dist;
  vector<double> frac;
  vector<math::XYZVector> tmp;
  while ( iter++ < niter && diff > 1E-8*ns2 ) {

    // Store previous iteration's result and reset pfclusters     
    ener.clear();
    tmp.clear();

    for ( unsigned ic=0; ic<curpfclusters.size(); ic++ ) {
      ener.push_back( curpfclusters[ic].energy() );
      
      math::XYZVector v;
      v = curpfclusters[ic].position();

      tmp.push_back( v );

#ifdef PFLOW_DEBUG
      if(debug_)  {
	cout<<"saving photon pos "<<ic<<" "<<curpfclusters[ic]<<endl;
	cout<<tmp[ic].X()<<" "<<tmp[ic].Y()<<" "<<tmp[ic].Z()<<endl;
      }
#endif

      curpfclusters[ic].reset();
    }


    // Loop over topocluster cells
    for( unsigned irh=0; irh<topocluster.size(); irh++ ) {
      
      unsigned rhindex = topocluster[irh];
      
      const reco::PFRecHit& rh = rechit( rhindex, rechits);
      
      // int layer = rh.layer();
             
      dist.clear();
      frac.clear();
      double fractot = 0.;

      bool isaseed = isSeed(rhindex);

      math::XYZVector cposxyzcell;
      cposxyzcell = rh.position();

#ifdef PFLOW_DEBUG
      if(debug_) { 
	cout<<rh<<endl;
	cout<<"start loop on curpfclusters"<<endl;
      }
#endif

      // Loop over pfclusters
      for ( unsigned ic=0; ic<tmp.size(); ic++) {
	
#ifdef PFLOW_DEBUG
	if(debug_) cout<<"pfcluster "<<ic<<endl;
#endif
	
	double frc=0.;
	bool seedexclusion=true;

	// convert cluster coordinates to xyz
	//math::XYZVector cposxyzclust( tmp[ic].X(), tmp[ic].Y(), tmp[ic].Z() );
        // cluster position used to compute distance with cell
	math::XYZVector cposxyzclust;
        cposxyzclust = curpfclusterswodepthcor[ic].position();

#ifdef PFLOW_DEBUG
	if(debug_) {
	  
	  cout<<"CLUSTER "<<cposxyzclust.X()<<","
	      <<cposxyzclust.Y()<<","
	      <<cposxyzclust.Z()<<"\t\t"
	      <<"CELL "<<cposxyzcell.X()<<","
	      <<cposxyzcell.Y()<<","
	      <<cposxyzcell.Z()<<endl;
	}  
#endif
	
	// Compute the distance between the current cell 
	// and the current PF cluster, normalized to a 
	// number of "sigma"
	math::XYZVector deltav = cposxyzclust;
	deltav -= cposxyzcell;
	double d = deltav.R() / showerSigma_;
	
	// if distance cell-cluster is too large, it means that 
	// we're right on the junction between 2 subdetectors (HCAL/VFCAL...)
	// in this case, distance is calculated in the xy plane
	// could also be a large supercluster... 
#ifdef PFLOW_DEBUG
	if( d > 10. && debug_ ) { 
          paint(rhindex, SPECIAL);
	  cout<<"PFClusterAlgo Warning: distance too large"<<d<<endl;
	}
#endif
	dist.push_back( d );

	// the current cell is the seed from the current photon.
	if( rhindex == seedsintopocluster[ic] && seedexclusion ) {
	  frc = 1.;
#ifdef PFLOW_DEBUG
	  if(debug_) cout<<"this cell is a seed for the current photon"<<endl;
#endif
	}
	else if( isaseed && seedexclusion ) {
	  frc = 0.;
#ifdef PFLOW_DEBUG
	  if(debug_) cout<<"this cell is a seed for another photon"<<endl;
#endif
	}
	else {
	  // Compute the fractions of the cell energy to be assigned to 
	  // each curpfclusters in the cluster.
	  frc = ener[ic] * exp ( - dist[ic]*dist[ic] / 2. );

#ifdef PFLOW_DEBUG
	  if(debug_) {
	    cout<<"dist["<<ic<<"] "<<dist[ic]
		<<", sigma="<<sigma
		<<", frc="<<frc<<endl;
	  }  
#endif
	
	}
	fractot += frc;
	frac.push_back(frc);
      }      

      // Add the relevant fraction of the cell to the curpfclusters
#ifdef PFLOW_DEBUG
      if(debug_) cout<<"start add cell"<<endl;
#endif
      for ( unsigned ic=0; ic<tmp.size(); ++ic ) {
#ifdef PFLOW_DEBUG
	if(debug_) 
	  cout<<" frac["<<ic<<"] "<<frac[ic]<<" "<<fractot<<" "<<rh<<endl;
#endif

	if( fractot ) 
	  frac[ic] /= fractot;
	else { 
#ifdef PFLOW_DEBUG
	  if( debug_ ) {
	    int layer = rh.layer();
	    cerr<<"fractot = 0 ! "<<layer<<endl;
	    
	    for( unsigned trh=0; trh<topocluster.size(); trh++ ) {
	      unsigned tindex = topocluster[trh];
	      const reco::PFRecHit& rh = rechit( tindex, rechits);
	      cout<<rh<<endl;
	    }

	    // assert(0)
	  }
#endif

	  continue;
	}

	// if the fraction has been set to 0, the cell 
	// is now added to the cluster - careful ! (PJ, 19/07/08)
	// BUT KEEP ONLY CLOSE CELLS OTHERWISE MEMORY JUST EXPLOSES
	// (PJ, 15/09/08 <- similar to what existed before the 
        // previous bug fix, but keeps the close seeds inside, 
	// even if their fraction was set to zero.)
	// Also add a protection to keep the seed in the cluster 
	// when the latter gets far from the former. These cases
	// (about 1% of the clusters) need to be studied, as 
	// they create fake photons, in general.
	// (PJ, 16/09/08) 
      	if ( dist[ic] < 10. || frac[ic] > 0.99999 ) { 
	  // if ( dist[ic] > 6. ) cout << "Warning : PCluster is getting very far from its seeding cell" << endl;
	  reco::PFRecHitRef  recHitRef = createRecHitRef( rechits, rhindex ); 
	  reco::PFRecHitFraction rhf( recHitRef,frac[ic] );
	  curpfclusters[ic].addRecHitFraction( rhf );
	}
      }
      // if(debug_) cout<<" end add cell"<<endl;
    }
    
    // Determine the new cluster position and check 
    // the distance with the previous iteration
    diff = 0.;
    for (  unsigned ic=0; ic<tmp.size(); ++ic ) {
      calculateClusterPosition( curpfclusters[ic], curpfclusterswodepthcor[ic], 
                                true, posCalcNCrystal );
#ifdef PFLOW_DEBUG
      if(debug_) cout<<"new iter "<<ic<<endl;
      if(debug_) cout<<curpfclusters[ic]<<endl;
#endif

      double delta = ROOT::Math::VectorUtil::DeltaR(curpfclusters[ic].position(),tmp[ic]);
      if ( delta > diff ) diff = delta;
    }
  }
  
  // Issue a warning message if the number of iterations 
  // exceeds 50
#ifdef PFLOW_DEBUG
  if ( iter >= 50 && debug_ ) 
    cout << "PFClusterAlgo Warning: "
	 << "more than "<<niter<<" iterations in pfcluster finding: " 
	 <<  setprecision(10) << diff << endl;
#endif
  
  // There we go
  // add all clusters to the list of pfClusters.
  for(unsigned ic=0; ic<curpfclusters.size(); ic++) {
    calculateClusterPosition(curpfclusters[ic], curpfclusterswodepthcor[ic], 
                             true, posCalcNCrystal);
    pfClusters_->push_back(curpfclusters[ic]); 
  }
}



void 
PFClusterAlgo::calculateClusterPosition(reco::PFCluster& cluster,
                                        reco::PFCluster& clusterwodepthcor,
					bool depcor, 
					int posCalcNCrystal) {

  if( posCalcNCrystal_ != -1 && 
      posCalcNCrystal_ != 5 && 
      posCalcNCrystal_ != 9 ) {
    throw "PFCluster::calculatePosition : posCalcNCrystal_ must be -1, 5, or 9.";
  }  

  if(!posCalcNCrystal) posCalcNCrystal = posCalcNCrystal_; 

  cluster.position_.SetXYZ(0,0,0);

  cluster.energy_ = 0;
  
  double normalize = 0;

  // calculate total energy, average layer, and look for seed  ---------- //

  // double layer = 0;
  map <PFLayer::Layer, double> layers; 
  unsigned seedIndex = 0;
  bool     seedIndexFound = false;

  //Colin: the following can be simplified!

  // loop on rechit fractions
  for (unsigned ic=0; ic<cluster.rechits_.size(); ic++ ) {

    unsigned rhi = cluster.rechits_[ic].recHitRef().index();
    // const reco::PFRecHit& rh = rechit( rhi, rechits );

    const reco::PFRecHit& rh = *(cluster.rechits_[ic].recHitRef());
    double fraction =  cluster.rechits_[ic].fraction();

    // Find the seed of this sub-cluster (excluding other seeds found in the topological
    // cluster, the energy fraction of which were set to 0 fpr the position determination.
    if( isSeed(rhi) && fraction > 1e-9 ) {
      seedIndex = rhi;
      seedIndexFound = true;
    }


    double recHitEnergy = rh.energy() * fraction;
    cluster.energy_ += recHitEnergy;

    // sum energy in each layer
    PFLayer::Layer layer = rh.layer();                              
    map <PFLayer::Layer, double>:: iterator it = layers.find(layer);
    if (it != layers.end()) 
      it->second += recHitEnergy;
    else 
      layers.insert(make_pair(layer, recHitEnergy));
  }  

  assert(seedIndexFound);

  // loop over pairs to find layer with max energy          
  double Emax = 0.;
  PFLayer::Layer layer = PFLayer::NONE;
  for (map<PFLayer::Layer, double>::iterator it = layers.begin();
       it != layers.end(); ++it) {
    double e = it->second;
    if(e > Emax){ 
      Emax = e; 
      layer = it->first;
    }
  }
  
  //setlayer here
  cluster.setLayer( layer ); // take layer with max energy

  // layer /= cluster.energy_;
  // cluster.layer_ = lrintf(layer); // nearest integer

  double p1 =  posCalcP1_;
  if( p1 < 0 ) { 
    // automatic (and hopefully best !) determination of the parameter
    // for position determination.
    
    // Remove the ad-hoc determination of p1, and set it to the 
    // seed threshold.
    switch(cluster.layer() ) {
    case PFLayer::ECAL_BARREL:
    case PFLayer::HCAL_BARREL1:
    case PFLayer::HCAL_BARREL2:
      p1 = threshBarrel_;
      break;
    case PFLayer::ECAL_ENDCAP:
    case PFLayer::HCAL_ENDCAP:
    case PFLayer::HF_EM:
    case PFLayer::HF_HAD:
    case PFLayer::PS1:
    case PFLayer::PS2:
      p1 = threshEndcap_;
      break;

    /*
    switch(cluster.layer() ) {
    case PFLayer::ECAL_BARREL:
      p1 = 0.004 + 0.022*cluster.energy_; // 27 feb 2006 
      break;
    case PFLayer::ECAL_ENDCAP:
      p1 = 0.014 + 0.009*cluster.energy_; // 27 feb 2006 
      break;
    case PFLayer::HCAL_BARREL1:
    case PFLayer::HCAL_BARREL2:
    case PFLayer::HCAL_ENDCAP:
    case PFLayer::HCAL_HF:
      p1 = 5.41215e-01 * log( cluster.energy_ / 1.29803e+01 );
      if(p1<0.01) p1 = 0.01;
      break;
    */

    default:
      cerr<<"Clusters weight_p1 -1 not yet allowed for layer "<<layer
	  <<". Chose a better value in the opt file"<<endl;
      assert(0);
      break;
    }
  } 
  else if( p1< 1e-9 ) { // will divide by p1 later on
    p1 = 1e-9;
  }

  // calculate uncorrected cluster position --------------------------------

  reco::PFCluster::REPPoint clusterpos;   // uncorrected cluster pos 
  math::XYZPoint clusterposxyz;           // idem, xyz coord 
  math::XYZPoint firstrechitposxyz;       // pos of the rechit with highest E

  double maxe = -9999;
  double x = 0;
  double y = 0;
  double z = 0;
  
  for (unsigned ic=0; ic<cluster.rechits_.size(); ic++ ) {
    
    unsigned rhi = cluster.rechits_[ic].recHitRef().index();
//     const reco::PFRecHit& rh = rechit( rhi, rechits );

    const reco::PFRecHit& rh = *(cluster.rechits_[ic].recHitRef());

    if(rhi != seedIndex) { // not the seed
      if( posCalcNCrystal == 5 ) { // pos calculated from the 5 neighbours only
	if(!rh.isNeighbour4(seedIndex) ) {
	  continue;
	}
      }
      if( posCalcNCrystal == 9 ) { // pos calculated from the 9 neighbours only
	if(!rh.isNeighbour8(seedIndex) ) {
	  continue;
	}
      }
    }
    
    double fraction =  cluster.rechits_[ic].fraction();
    double recHitEnergy = rh.energy() * fraction;

    double norm = fraction < 1E-9 ? 0. : max(0., log(recHitEnergy/p1 ));
    
    const math::XYZPoint& rechitposxyz = rh.position();
    
    if( recHitEnergy > maxe ) {
      firstrechitposxyz = rechitposxyz;
      maxe = recHitEnergy;
    }

    x += rechitposxyz.X() * norm;
    y += rechitposxyz.Y() * norm;
    z += rechitposxyz.Z() * norm;
    
    // clusterposxyz += rechitposxyz * norm;
    normalize += norm;
  }
  
  // normalize uncorrected position
  // assert(normalize);
  if( normalize < 1e-9 ) {
    //    cerr<<"--------------------"<<endl;
    //    cerr<<(*this)<<endl;
    cout << "Watch out : cluster too far from its seeding cell, set to 0,0,0" << endl;
    clusterposxyz.SetXYZ(0,0,0);
    clusterpos.SetCoordinates(0,0,0);
    return;
  }
  else {
    x /= normalize;
    y /= normalize; 
    z /= normalize; 
    clusterposxyz.SetCoordinates( x, y, z);
    clusterpos.SetCoordinates( clusterposxyz.Rho(), clusterposxyz.Eta(), clusterposxyz.Phi() );
  }  

  cluster.posrep_ = clusterpos;
  cluster.position_ = clusterposxyz;

  clusterwodepthcor = cluster;


  // correction of the rechit position, 
  // according to the depth, only for ECAL 


  if( depcor &&   // correction requested and ECAL
      ( cluster.layer() == PFLayer::ECAL_BARREL ||       
	cluster.layer() == PFLayer::ECAL_ENDCAP ) ) {

    
    double corra = reco::PFCluster::depthCorA_;
    double corrb = reco::PFCluster::depthCorB_;
    if( abs(clusterpos.Eta() ) < 2.6 && 
	abs(clusterpos.Eta() ) > 1.65   ) { 
      // if crystals under preshower, correction is not the same  
      // (shower depth smaller)
      corra = reco::PFCluster::depthCorAp_;
      corrb = reco::PFCluster::depthCorBp_;
    }

    double depth = 0;

    switch( reco::PFCluster::depthCorMode_ ) {
    case 1: // for e/gamma 
      depth = corra * ( corrb + log(cluster.energy_) ); 
      break;
    case 2: // for hadrons
      depth = corra;
      break;
    default:
      cerr<<"PFClusterAlgo::calculateClusterPosition : unknown function for depth correction! "<<endl;
      assert(0);
    }

    // calculate depth vector:
    // its mag is depth
    // its direction is the cluster direction (uncorrected)

//     double xdepthv = clusterposxyz.X();
//     double ydepthv = clusterposxyz.Y();
//     double zdepthv = clusterposxyz.Z();
//     double mag = sqrt( xdepthv*xdepthv + 
// 		       ydepthv*ydepthv + 
// 		       zdepthv*zdepthv );
    

//     math::XYZPoint depthv(clusterposxyz); 
//     depthv.SetMag(depth);
    
    
    math::XYZVector depthv( clusterposxyz.X(), 
			    clusterposxyz.Y(),
			    clusterposxyz.Z() );
    depthv /= sqrt(depthv.Mag2() );
    depthv *= depth;


    // now calculate corrected cluster position:    
    math::XYZPoint clusterposxyzcor;

    maxe = -9999;
    x = 0;
    y = 0;
    z = 0;
    cluster.posrep_.SetXYZ(0,0,0);
    normalize = 0;
    for (unsigned ic=0; ic<cluster.rechits_.size(); ic++ ) {

      unsigned rhi = cluster.rechits_[ic].recHitRef().index();
//       const reco::PFRecHit& rh = rechit( rhi, rechits );
      
      const reco::PFRecHit& rh = *(cluster.rechits_[ic].recHitRef());

      if(rhi != seedIndex) {
	if( posCalcNCrystal == 5 ) {
	  if(!rh.isNeighbour4(seedIndex) ) {
	    continue;
	  }
	}
	if( posCalcNCrystal == 9 ) {
	  if(!rh.isNeighbour8(seedIndex) ) {
	    continue;
	  }
	}
      }
    
      double fraction =  cluster.rechits_[ic].fraction();
      double recHitEnergy = rh.energy() * fraction;
      
      const math::XYZPoint&  rechitposxyz = rh.position();

      // rechit axis not correct ! 
      math::XYZVector rechitaxis = rh.getAxisXYZ();
      // rechitaxis -= math::XYZVector( rechitposxyz.X(), rechitposxyz.Y(), rechitposxyz.Z() );
      
      math::XYZVector rechitaxisu( rechitaxis );
      rechitaxisu /= sqrt( rechitaxis.Mag2() );

      math::XYZVector displacement( rechitaxisu );
      // displacement /= sqrt( displacement.Mag2() );    
      displacement *= rechitaxisu.Dot( depthv );
      
      math::XYZPoint rechitposxyzcor( rechitposxyz );
      rechitposxyzcor += displacement;

      if( recHitEnergy > maxe ) {
	firstrechitposxyz = rechitposxyzcor;
	maxe = recHitEnergy;
      }

      double norm = fraction < 1E-9 ? 0. : max(0., log(recHitEnergy/p1 ));
      
      x += rechitposxyzcor.X() * norm;
      y += rechitposxyzcor.Y() * norm;
      z += rechitposxyzcor.Z() * norm;
      
      // clusterposxyzcor += rechitposxyzcor * norm;
      normalize += norm;
    }
    
    // normalize
    if(normalize < 1e-9) {
      cerr<<"--------------------"<<endl;
      cerr<< cluster <<endl;
      assert(0);
    }
    else {
      x /= normalize;
      y /= normalize;
      z /= normalize;
      

      clusterposxyzcor.SetCoordinates(x,y,z);
      cluster.posrep_.SetCoordinates( clusterposxyzcor.Rho(), 
				      clusterposxyzcor.Eta(), 
				      clusterposxyzcor.Phi() );
      cluster.position_  = clusterposxyzcor;
      clusterposxyz = clusterposxyzcor;
    }
  }
}



const reco::PFRecHit& 
PFClusterAlgo::rechit(unsigned i, 
		      const reco::PFRecHitCollection& rechits ) {

  if( i < 0 || i >= rechits.size() ) {
    string err = "PFClusterAlgo::rechit : out of range";
    throw std::out_of_range(err);
  }
  
  return rechits[i];
}



bool PFClusterAlgo::masked(unsigned rhi) const {

  if(rhi<0 || rhi>=mask_.size() ) {
    string err = "PFClusterAlgo::masked : out of range";
    throw std::out_of_range(err);
  }
  
  return mask_[rhi];
}


unsigned PFClusterAlgo::color(unsigned rhi) const {

  if(rhi<0 || rhi>=color_.size() ) {
    string err = "PFClusterAlgo::color : out of range";
    throw std::out_of_range(err);
  }
  
  return color_[rhi];
}



bool PFClusterAlgo::isSeed(unsigned rhi) const {

  if(rhi<0 || rhi>=seedStates_.size() ) {
    string err = "PFClusterAlgo::isSeed : out of range";
    throw std::out_of_range(err);
  }
  
  return seedStates_[rhi]>0 ? true : false;
}


void PFClusterAlgo::paint(unsigned rhi, unsigned color ) {

  if(rhi<0 || rhi>=color_.size() ) {
    string err = "PFClusterAlgo::color : out of range";
    throw std::out_of_range(err);
  }
  
  color_[rhi] = color;
}


reco::PFRecHitRef 
PFClusterAlgo::createRecHitRef( const reco::PFRecHitCollection& rechits, 
				unsigned rhi ) {

  if( rechitsHandle_.isValid() ) {
    return reco::PFRecHitRef(  rechitsHandle_, rhi );
  } 
  else {
    return reco::PFRecHitRef(  &rechits, rhi );
  }
}


ostream& operator<<(ostream& out,const PFClusterAlgo& algo) {
  if(!out) return out;
  out<<"PFClusterAlgo parameters : "<<endl;
  out<<"-----------------------------------------------------"<<endl;
  out<<"threshBarrel       : "<<algo.threshBarrel_       <<endl;
  out<<"threshSeedBarrel   : "<<algo.threshSeedBarrel_   <<endl;
  out<<"threshPtBarrel     : "<<algo.threshPtBarrel_     <<endl;
  out<<"threshPtSeedBarrel : "<<algo.threshPtSeedBarrel_ <<endl;
  out<<"threshCleanBarrel  : "<<algo.threshCleanBarrel_  <<endl;
  out<<"minS4S1Barrel      : "<<algo.minS4S1Barrel_      <<endl;
  out<<"threshEndcap       : "<<algo.threshEndcap_       <<endl;
  out<<"threshSeedEndcap   : "<<algo.threshSeedEndcap_   <<endl;
  out<<"threshPtEndcap     : "<<algo.threshPtEndcap_     <<endl;
  out<<"threshPtSeedEndcap : "<<algo.threshPtSeedEndcap_ <<endl;
  out<<"threshEndcap       : "<<algo.threshEndcap_       <<endl;
  out<<"threshCleanEndcap  : "<<algo.threshCleanEndcap_  <<endl;
  out<<"minS4S1Endcap      : "<<algo.minS4S1Endcap_      <<endl;
  out<<"nNeighbours        : "<<algo.nNeighbours_        <<endl;
  out<<"posCalcNCrystal    : "<<algo.posCalcNCrystal_    <<endl;
  out<<"posCalcP1          : "<<algo.posCalcP1_          <<endl;
  out<<"showerSigma        : "<<algo.showerSigma_        <<endl;
  out<<"useCornerCells     : "<<algo.useCornerCells_     <<endl;

  out<<endl;
  out<<algo.pfClusters_->size()<<" clusters:"<<endl;

  for(unsigned i=0; i<algo.pfClusters_->size(); i++) {
    out<<(*algo.pfClusters_)[i]<<endl;
    
    if(!out) return out;
  }
  
  return out;
}
