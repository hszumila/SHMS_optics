// Standard includes.
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
  using std::cin;
  using std::cout;
  using std::endl;
#include <thread>
#include <utility>
#include <vector>

// ROOT includes.
#include "TCanvas.h"
#include "TDecompSVD.h"
#include "TDirectory.h"
#include "TEllipse.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TLine.h"
#include "TMarker.h"
#include "TMatrixD.h"
#include "TRint.h"
#include "TString.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TVectorD.h"
#include "TGraph.h"
#include "TF1.h"

// Project includes.
#include "cmdOptions.hpp"
#include "myConfig.hpp"
#include "myEvent.hpp"
#include "myMath.hpp"
#include "myOther.hpp"
#include "myRecMatrix.hpp"


int shms_optics(const cmdOptions::OptionParser_shmsOptics& cmdOpts);


int main(int argc, char* argv[]) {
  // Parse command line options for shms_optics.
  cmdOptions::OptionParser_shmsOptics cmdOpts;
  try {
    cmdOpts.init(argc, argv);
  }
  catch (const std::runtime_error& err) {
    cout << "shms_optics: " << err.what() << endl;
    cout << "shms_optics: Try `shms_optics -h` for more information." << endl;
    return 1;
  }
  if (cmdOpts.displayHelp) {
    cmdOpts.printHelp();
    return 0;
  }

  // Create command line options for ROOT.
  int argcRoot = 3;
  static char argvRoot[][100] = {"-q", "-l"};
  static char* argvRootList[] = {argv[0], argvRoot[0], argvRoot[1], NULL};

  // Run shms_optics as an application.
  TRint *theApp = new TRint("app", &argcRoot, argvRootList);
  int retCode = shms_optics(cmdOpts);
  theApp->Run(kTRUE);

  // Cleanup and exit.
  delete theApp;

  return retCode;
}


int shms_optics(const cmdOptions::OptionParser_shmsOptics& cmdOpts) {

  gStyle->SetOptFit(0000);
  gStyle->SetOptStat("");

  cout
    << "Reading config file:" << endl
    << "  `" << cmdOpts.configFileName << "`" << endl;
  config::Config conf = config::loadConfigFile(cmdOpts.configFileName);

  std::string recMatrixDepFileName = conf.recMatrixFileNameOld;
  recMatrixDepFileName.insert(
    conf.recMatrixFileNameOld.size()-4, "__dep"
  );
  std::string recMatrixIndepFileName = conf.recMatrixFileNameOld;
  recMatrixIndepFileName.insert(
    conf.recMatrixFileNameOld.size()-4, "__indep"
  );
  cout
    << "Reading xTar dependent matrix file:" << endl
    << "  `" << recMatrixDepFileName << "`" << endl;
  RecMatrix recMatrixDep = readMatrixFile(recMatrixDepFileName);
  cout
    << "Reading xTar independent matrix file:" << endl
    << "  `" << recMatrixIndepFileName << "`" << endl;
  RecMatrix recMatrixIndep = readMatrixFile(recMatrixIndepFileName);


  cout << "Initializing new xTar independent matrix." << endl;
  // Copy header and delta elements from old matrix.
  // Initialize other elements to 0.
  RecMatrix recMatrixNew;
  recMatrixNew.header = recMatrixIndep.header;
  double C_D;
  std::vector<RecMatrixLine>::iterator lineIt;
  // Construct order by order.
  // Only include xTar independent terms.
  for (int order=0; order<=conf.fitOrder; ++order) {
    for (int l=0; l<=order; ++l) {
      for (int k=0; k<=order-l; ++k) {
        for (int j=0; j<=order-l-k; ++j) {
          for (int i=0; i<=order-l-k-j; ++i) {
            if (i+j+k+l != order) continue;

            lineIt = recMatrixIndep.findLine(i, j, k, l, 0);
            if (lineIt != recMatrixIndep.end()) C_D = lineIt->C_D;
            else C_D = 0.0;

            recMatrixNew.addLine(
              0.0, 0.0, 0.0, C_D,
              i, j, k, l, 0
            );
          }
        }
      }
    }
  }
  int recMatrixNewLen = static_cast<int>(recMatrixNew.size());
  cout << "  " << recMatrixNewLen << " xTar independent terms" << endl;


  // Prepare for analysis.
  char tmp;

  TFile fo(cmdOpts.rootFileName.c_str(), "RECREATE");
  TDirectory* dir;

  TMatrixD xpTarFitMat(recMatrixNewLen, recMatrixNewLen);
  TMatrixD yTarFitMat(recMatrixNewLen, recMatrixNewLen);
  TMatrixD ypTarFitMat(recMatrixNewLen, recMatrixNewLen);
  TVectorD xpTarFitVec(recMatrixNewLen);
  TVectorD yTarFitVec(recMatrixNewLen);
  TVectorD ypTarFitVec(recMatrixNewLen);

  TCanvas* c1 = new TCanvas("c1", "c1", 100, 100, 600, 400);
  TCanvas* c2 = new TCanvas("c2", "c2", 100, 540, 600, 400);
  TCanvas* c3 = new TCanvas("c3", "c3", 702, 100, 600, 400);
  gPad->Update();

 

  //make 1D histos for each x and y holes:
  TH1F *h_ytar_ysieve[3][12];
  TH1F *h_yptar_ysieve[3][12];
  TH1F *h_xptar_xsieve[3][12];

  cout << "Reading and analyzing root files:" << endl;
  for (const auto& runConf : conf.runConfigs) {  // run loop
    cout << "  " << runConf.runNumber << ":" << endl;

    //make 2D plots for xpTar and ypTar
    TH2D *h2_xpTar = new TH2D("h2_xpTar",";xpTar_{real};xpTar_{measured} - xpTar_{real}",200,0.0,0.06,200,-0.01,0.01);
    TH2D *h2_ypTar = new TH2D("h2_ypTar",";ypTar_{real};ypTar_{measured} - ypTar_{real}",200,0.0,0.06,200,-0.01,0.01);
    TH2D *h2_yTar = new TH2D("h2_yTar",";yTar_{real};yTar_{measured} - yTar_{real}",200,-6.0,6.0,200,-3.0,3.0);
    TH2D *h2_zVer = new TH2D("h2_zVer",";zVer_{real};zVer_{measured} - zVer_{real}",200,-12.0,12.0,200,-5.0,5.0);
    TH2D *h2_yTarVypTar = new TH2D("h2_yTarVypTar",";yTar [cm]; ypTar",200,-4,4,200,-0.05,0.05);
    TH2F *h2_yTarVdelta = new TH2F("h2_yTarVdelta",";yTar measured [cm];delta",200,-6.0,6.0,200,-15,20);
    TH2F *h2_yTarVdelta_cut = new TH2F("h2_yTarVdelta_cut","With cuts;yTar measured [cm];delta",200,-6.0,6.0,200,-15,20);
    TH2F *h2_fp = new TH2F("h2_fp",";xfp [cm]; yfp [cm]",200,0,8,200,-15,15);

    const size_t nFoils = runConf.zFoils.size();

    std::vector<double> xSievePhys = runConf.getSieveHolesX();
    std::vector<double> ySievePhys = runConf.getSieveHolesY();

    //make plots for sieve holes:
    const size_t ixSieve = runConf.sieve.nRow;
    const size_t iySieve = runConf.sieve.nCol;

    for (uint iif=0; iif<nFoils; iif++){
      for (uint ii=0; ii<ixSieve; ii++){
	h_xptar_xsieve[iif][ii] = new TH1F(Form("h_xptar_xsieve_%d_%d",iif,ii),Form("xptar residual foil %d, hole %d",iif, ii),200,-0.009,0.009);
      }
      for (uint ii=0; ii<iySieve; ii++){
	h_yptar_ysieve[iif][ii] = new TH1F(Form("h_yptar_ysieve_%d_%d",iif,ii),Form("yptar residual foil %d, hole %d",iif, ii),200,-0.009,0.009);
	h_ytar_ysieve[iif][ii] = new TH1F(Form("h_ytar_ysieve_%d_%d",iif,ii),Form("ytar residual foil %d, hole %d",iif, ii),200,-0.01,0.01);
      }      
    }
    

    // Reading events from input ROOT files.
    //cout<<"Made it this far!"<<endl;
    std::vector<Event> events = readEvents(runConf);
    //cout<<"Got lost reading events"<<endl;
    size_t nEvents = events.size();
    cout << "    " << nEvents << " events survived cuts." << endl;

    fo.cd();
    // Create directory in output ROOT file for histograms.
    dir = fo.mkdir(
      TString::Format("run_%d", runConf.runNumber),
      TString::Format("histograms for run %d", runConf.runNumber)
    );
    dir->cd();

    cout << "    Reconstructing events: ";
    size_t iEvent = 0;
    double xpSumIndep, xpSumDep;
    double ySumIndep, ySumDep;
    double ypSumIndep, ypSumDep;
    double lambda;
    const double D1 = 138.0;
    const double D2 = 75.0;
    const double D3 = 40.0;

    reportProgressInit();
    for (auto& event : events) {  // reconstruction event loop
      if (iEvent%2000 == 0) reportProgress(iEvent, nEvents);

      double cosTheta = cos(event.theta*TMath::DegToRad());
      double sinTheta = sin(event.theta*TMath::DegToRad());


      xpSumIndep = 0.0;
      ySumIndep = 0.0;
      ypSumIndep = 0.0;
      xpSumDep = 0.0;
      ySumDep = 0.0;
      ypSumDep = 0.0;
      lambda = 0.0;

      h2_fp->Fill(event.xFp,event.yFp);

      if (iEvent==1){
	//cout<<"------------Matrix Elements Indep-------------"<<endl;
      }

      // Calculate contribution of xTar independent terms.
      for (const auto& line : recMatrixIndep.matrix) {  // line loop
        lambda =
          pow(event.xFp/100.0, line.E_x) *
          pow(event.xpFp, line.E_xp) *
          pow(event.yFp/100.0, line.E_y) *
          pow(event.ypFp, line.E_yp);

        xpSumIndep += line.C_Xp * lambda;
        ySumIndep += line.C_Y * lambda;
        ypSumIndep += line.C_Yp * lambda;

	if (iEvent==1){
	  //cout<<line.C_Xp<<"\t"<<line.C_Y<<"\t"<<line.C_Yp<<"\t"<<line.E_x<<line.E_xp<<line.E_y<<line.E_yp<<endl;
	}
      }   // line loop

      // Now do several iterations of xTar dependent constributions, each time
      // with a better approximation for xTar.
      event.xTar = -event.yVer - runConf.SHMS.xMispointing;
      double corrFactor = 25.0;
      double uncorrYTar = 0.0;
      double uncorrZVer = 0.0;
      for (int iIter=0; iIter<conf.xTarCorrIterNum+1; ++iIter) {  // iteration loop
        xpSumDep = 0.0;
        ySumDep = 0.0;
        ypSumDep = 0.0;

	if (iEvent==1){
	  //cout<<"------------Matrix Elements Dep-------------"<<endl;
	}

        for (const auto& line : recMatrixDep.matrix) {  // line loop
          lambda =
            pow(event.xFp/100.0, line.E_x) *
            pow(event.xpFp, line.E_xp) *
            pow(event.yFp/100.0, line.E_y) *
            pow(event.ypFp, line.E_yp) *
            pow(event.xTar/100.0, line.E_xTar);

          xpSumDep += line.C_Xp * lambda;
          ySumDep += line.C_Y * lambda;
          ypSumDep += line.C_Yp * lambda;

	  if (iEvent==1){
	    //cout<<line.C_Xp<<"\t"<<line.C_Y<<"\t"<<line.C_Yp<<"\t"<<line.E_x<<line.E_xp<<line.E_y<<line.E_yp<<line.E_xTar<<endl;
	  }
        }  // line loop

        event.xpTar = (xpSumIndep+xpSumDep) + runConf.SHMS.phiOffset;
        event.yTar = (ySumIndep+ySumDep)*100.0 + runConf.SHMS.yMispointing;
        event.ypTar = (ypSumIndep+ypSumDep) + runConf.SHMS.thetaOffset;

	//correct the ytar vs yptar dependency
	//this is for 2017 data prior to optimization only
	uncorrYTar = event.yTar;
	if (sinTheta>0.4){corrFactor = 6.0;}
	
	if (runConf.use2017Corr != 0){
	  event.yTar = event.yTar - runConf.SHMS.yMispointing - corrFactor*event.ypTar;
	  event.yTar += runConf.SHMS.yMispointing;
	}
	
	event.zVer =
          (event.yTar - event.xVer*(cosTheta - event.ypTar*sinTheta)) /
          (-sinTheta - event.ypTar*cosTheta);
	
	uncorrZVer = (uncorrYTar - event.xVer*(cosTheta - event.ypTar*sinTheta)) /
	  (-sinTheta - event.ypTar*cosTheta);
	

        event.xTarVer = -event.yVer;	
	event.yTarVer = -event.zVer*sinTheta + event.xVer*cosTheta;
        event.zTarVer = event.zVer*cosTheta + event.xVer*sinTheta;

	event.yTarVer = -uncorrZVer*sinTheta + event.xVer*cosTheta;
        event.zTarVer = uncorrZVer*cosTheta + event.xVer*sinTheta;


        event.xTar = event.xTarVer - event.zTarVer*event.xpTar - runConf.SHMS.xMispointing;
      }   // iteration loop

      event.xTar += runConf.SHMS.xMispointing;
      event.yTar -= runConf.SHMS.yMispointing;
      h2_yTarVypTar->Fill(event.yTar,event.ypTar);

      event.xSieve = event.xTar + event.xpTar*runConf.sieve.z0;
      event.ySieve = (-0.019*event.delta+0.00019*pow(event.delta,2)+(D1+D2)*event.ypTar+uncorrYTar) + D3*(-0.00052*event.delta+0.0000052*pow(event.delta,2)+event.ypTar);
      h2_yTarVdelta->Fill(event.yTar, event.delta);

      ++iEvent;
    }  // reconstruction event loop
    reportProgressFinish();


    cout << "    Fitting target foils." << endl;

    // Setting historgams.
    double minx = runConf.zFoils.front() - 5.0;
    double maxx = runConf.zFoils.back() + 5.0;
    int binsx = 10 * static_cast<int>(maxx-minx);
    TH1D zVerHist(
      TString::Format("zVer"),
      TString::Format("zVer for run %d", runConf.runNumber),
      binsx, minx, maxx
    );
    zVerHist.GetXaxis()->SetTitle("z_{vertex}  [cm]");
    
    minx *= runConf.SHMS.sinTheta;
    maxx *= runConf.SHMS.sinTheta;
    TH1D yTarHist(
      TString::Format("yTar"),
      TString::Format("yTar for run %d", runConf.runNumber),
      binsx, minx, maxx
    );
    yTarHist.GetXaxis()->SetTitle("y_{target}  [cm]");

    // Filling the histograms.
    for (const auto& event : events) {
      zVerHist.Fill(event.zVer);
      yTarHist.Fill(event.yTar);
    }

    // Fitting the histograms.
    int nnFoils = (int)nFoils;

    std::vector<Peak> zVerPeaks = findPeaks(&zVerHist, nnFoils);
    std::vector<Peak> yTarPeaks = findPeaks(&yTarHist, nnFoils);
    zVerHist.GetXaxis()->SetRange(1,binsx);
    yTarHist.GetXaxis()->SetRange(1,binsx);

    //std::vector<Peak> yTarPeaks = findPeaks(&yTarHist, nnFoils);

    //std::vector<Peak> zVerPeaks = fitMultiPeak(&zVerHist, 0.5);
    /*
    int nnFoils = (int)nFoils;
    std::vector<Peak> zVerPeaks = selectMultiPeakZ(&zVerHist, nnFoils, sinTheta);
    std::vector<Peak> yTarPeaks = selectMultiPeakY(&yTarHist, nnFoils, sinTheta);
    */
    //std::vector<Peak> yTarPeaks = fitMultiPeak(&yTarHist, 0.7);
    
    cout<<"Number of foils found: "<<zVerPeaks.size()<<endl;
    for (uint kk=0; kk<zVerPeaks.size(); kk++){
      cout<<"   peak: "<<zVerPeaks.at(kk).mean<<" , width: "<<zVerPeaks.at(kk).sigma<<" height: "<<zVerPeaks.at(kk).norm<<endl;
    }
    cout<<"Number of yTar found: "<<yTarPeaks.size()<<endl;
    
    for (uint kk=0; kk<yTarPeaks.size(); kk++){
      cout<<"   peak: "<<yTarPeaks.at(kk).mean<<" , width: "<<yTarPeaks.at(kk).sigma<<" height: "<<yTarPeaks.at(kk).norm<<endl;
    }
    
    // Plotting the histograms.
    c1->cd();
    zVerHist.Draw();
    c1->Update();
    double miny = gPad->GetUymin();
    double maxy = gPad->GetUymax();
    // Add lines for physical positions of foils.
    std::vector<TLine> zFoilLines(nFoils);
    for (size_t iFoil=0; iFoil<nFoils; ++iFoil) {
      zFoilLines.at(iFoil) = TLine(
        runConf.zFoils.at(iFoil), miny,
        runConf.zFoils.at(iFoil), maxy
      );
      zFoilLines.at(iFoil).SetLineColor(6);
      zFoilLines.at(iFoil).SetLineWidth(2);
      zVerHist.GetListOfFunctions()->Add(&(zFoilLines.at(iFoil)));
    }
    c1->Update();
    gPad->Update();
    zVerHist.Write();

    c3->cd();
    yTarHist.Draw();
    c3->Update();
    miny = gPad->GetUymin();
    maxy = gPad->GetUymax();
    std::vector<TLine> yTarLines(nFoils);
    for (size_t iFoil=0; iFoil<nFoils; ++iFoil) {
      double xVer = -runConf.beam.x0;//?
      double yTarVer = -runConf.zFoils.at(iFoil)*runConf.SHMS.sinTheta + xVer*runConf.SHMS.cosTheta - runConf.SHMS.yMispointing;
      double zTarVer = runConf.zFoils.at(iFoil)*runConf.SHMS.cosTheta + xVer*runConf.SHMS.sinTheta;
      double ypTar = (0 - yTarVer)/(253.0 - zTarVer);
      Double_t yTarZ = yTarVer - ypTar*zTarVer; 

      yTarLines.at(iFoil) = TLine(
        yTarZ, miny,
        yTarZ, maxy
      );
      yTarLines.at(iFoil).SetLineColor(6);
      yTarLines.at(iFoil).SetLineWidth(2);
      yTarHist.GetListOfFunctions()->Add(&(yTarLines.at(iFoil)));
    }
    c3->Update();
    gPad->Update();
    yTarHist.Write();

    if (cmdOpts.automatic) {
      //      std::this_thread::sleep_for(std::chrono::milliseconds(cmdOpts.delay));
    }
    else {
      cout << "    Continue? ";
      cin >> tmp;
    }

    c1->Clear();
    gPad->Update();
    c3->Clear();
    gPad->Update();
    
    cout << "    Fitting sieve holes." << endl;
    
    std::vector<TH2D> xySieveHists(nFoils);
    std::vector<TLine> xSieveLines(runConf.sieve.nRow);
    std::vector<TLine> ySieveLines(runConf.sieve.nCol);
    
    minx = xSievePhys.front() - 0.1*(xSievePhys.back()-xSievePhys.front());
    maxx = xSievePhys.back() + 0.1*(xSievePhys.back()-xSievePhys.front());
    binsx = 10 * static_cast<int>(maxx-minx);
    miny = ySievePhys.front() - 0.1*(ySievePhys.back()-ySievePhys.front());
    maxy = ySievePhys.back() + 0.1*(ySievePhys.back()-ySievePhys.front());
    int binsy = 10 * static_cast<int>(maxy-miny);
    
    // Construct lines for physical positions of sieve holes.
    for (size_t iRow=0; iRow<runConf.sieve.nRow; ++iRow) {
      xSieveLines.at(iRow) = TLine(
				   xSievePhys.at(iRow), miny,
				   xSievePhys.at(iRow), maxy
				   );
      xSieveLines.at(iRow).SetLineColor(6);
      xSieveLines.at(iRow).SetLineWidth(2);
    }
    for (size_t iCol=0; iCol<runConf.sieve.nCol; ++iCol) {
      ySieveLines.at(iCol) = TLine(
				   minx, ySievePhys.at(iCol),
				   maxx, ySievePhys.at(iCol)
				   );
      ySieveLines.at(iCol).SetLineColor(6);
      ySieveLines.at(iCol).SetLineWidth(2);
    }
    
   

    // Setting histograms.
    for (size_t iCol=0; iCol<runConf.sieve.nCol; ++iCol) {
      ySieveLines.at(iCol) = TLine(
				   minx, ySievePhys.at(iCol),
				   maxx, ySievePhys.at(iCol)
				   );
      ySieveLines.at(iCol).SetLineColor(6);
      ySieveLines.at(iCol).SetLineWidth(2);
    }
    
    
    TH2D *h2_xSieveAng[nFoils];
    TH2D *h2_ySieveAng[nFoils];

    for (size_t iFoil=0; iFoil<nFoils; ++iFoil) {
      h2_xSieveAng[iFoil] = new TH2D(Form("h2_xSieveAng_%d",static_cast<int>(iFoil)),Form("Run %d Foil %d;xSieve_{real};ypTar_{measured} - ypTar_{real}",runConf.runNumber, static_cast<int>(iFoil)),200,-12.0,12.0,200,-0.02,0.02);
      h2_ySieveAng[iFoil] = new TH2D(Form("h2_ySieveAng_%d",static_cast<int>(iFoil)),Form("Run %d Foil %d;ySieve_{real};xptar_{measured} - xpTar_{real}",runConf.runNumber,static_cast<int>(iFoil)),200,-7.0,7.0,200,-0.02,0.02);

      xySieveHists.at(iFoil) = TH2D(
				    TString::Format("xySieve_%d", static_cast<int>(iFoil)),
				    TString::Format("xySieve for foil %d run %d", static_cast<int>(iFoil), runConf.runNumber),
				    binsx, minx, maxx,
				    binsy, miny, maxy
				    );
      xySieveHists.at(iFoil).GetXaxis()->SetTitle("x_{sieve}  [cm]");
      xySieveHists.at(iFoil).GetYaxis()->SetTitle("y_{sieve}  [cm]");
      for (auto& line : xSieveLines) {
	xySieveHists.at(iFoil).GetListOfFunctions()->Add(&line);
      }
      for (auto& line : ySieveLines) {
	xySieveHists.at(iFoil).GetListOfFunctions()->Add(&line);
      }
    }
  
    // Filling the histograms.
    for (const auto& event : events) {
      for (size_t iFoil=0; iFoil<nFoils; ++iFoil) {
	double zVerSigma = zVerPeaks.at(iFoil).sigma;
	if (
	    zVerPeaks.at(iFoil).mean - 1.3*zVerSigma <= event.zVer &&
	    event.zVer <= zVerPeaks.at(iFoil).mean + 1.3*zVerSigma &&
	    ((event.delta<1&&(yTarPeaks.at(nFoils-1-iFoil).mean - 1.0*yTarPeaks.at(nFoils-1-iFoil).sigma <= event.yTar &&
			      event.yTar <= yTarPeaks.at(nFoils-1-iFoil).mean + 1.0*yTarPeaks.at(nFoils-1-iFoil).sigma))||
	     (event.delta>=1&&(yTarPeaks.at(nFoils-1-iFoil).mean - 1.8*yTarPeaks.at(nFoils-1-iFoil).sigma <= event.yTar &&
			       event.yTar <= yTarPeaks.at(nFoils-1-iFoil).mean + 1.8*yTarPeaks.at(nFoils-1-iFoil).sigma)))
	    &&event.delta>-12
	    ) {
	  h2_yTarVdelta_cut->Fill(event.yTar, event.delta);
	  xySieveHists.at(iFoil).Fill(event.xSieve, event.ySieve);
	  break;
	}
      }
    }

    // Setting things before starting.
    std::vector<std::vector<Peak> > xSievePeakss(nFoils);
    std::vector<std::vector<Peak> > ySievePeakss(nFoils);
    std::vector<std::vector<std::size_t> > xSieveIndexess(nFoils);
    std::vector<std::vector<std::size_t> > ySieveIndexess(nFoils);
    std::vector<std::vector<std::size_t> > nEventss(nFoils);
    std::vector<std::vector<TEllipse> > ellipsess(nFoils);
    
    TH1D* tmpHist;
    TMarker* tmpMark = new TMarker(0.0, 0.0, 22);
    tmpMark->SetMarkerColor(2);
    
    // Fit sieve holes for each foil.
    for (size_t iFoil=0; iFoil<nFoils; ++iFoil) {  // foil loop
      cout << "      Foil " << iFoil << "." << endl;
      //if (iFoil<2) continue;
      TH2D& xySieveHist = xySieveHists.at(iFoil);
      
      c1->cd();
      xySieveHist.Draw("colz");
      c1->Update();
      gPad->Update();
      
      // Fit the projections to get position estimates.
      c2->cd();
      if (iFoil>=1){
	tmpHist = xySieveHist.ProjectionX("",binsy/4,binsy/1,"");
      }
      else{
	tmpHist = xySieveHist.ProjectionX();
      }
      tmpHist->SetTitle("x_{fp} projection");
      tmpHist->Draw();
      std::vector<Peak> xSievePeaksFit = fitMultiPeak(tmpHist, 0.1);
      gPad->Update();
      
      c3->cd();
      if (iFoil>=1){
	tmpHist = xySieveHist.ProjectionY("", binsx/4,binsx/1,"");				       
      }
      else{
	tmpHist = xySieveHist.ProjectionY();				       
      }
      tmpHist->SetTitle("y_{fp} projection");
      tmpHist->Draw();
      std::vector<Peak> ySievePeaksFit = fitMultiPeak(tmpHist, 0.1);
      gPad->Update();

      // Setup before fitting.
      
      std::vector<Peak>& xSievePeaks = xSievePeakss.at(iFoil);
      std::vector<Peak>& ySievePeaks = ySievePeakss.at(iFoil);
      std::vector<std::size_t>& xSieveIndexes = xSieveIndexess.at(iFoil);
      std::vector<std::size_t>& ySieveIndexes = ySieveIndexess.at(iFoil);
      std::vector<std::size_t>& nEvents = nEventss.at(iFoil);
      std::vector<TEllipse>& ellipses = ellipsess.at(iFoil);

      // Fit each individual hole.      
      double xComparison = -30.0;
      for (const auto& xSievePeak : xSievePeaksFit) {
	tmpMark->SetX(xSievePeak.mean);
	double xPeakSigmaInit = 0.36;//xSievePeak.sigma;
	if (xPeakSigmaInit>0.36){xPeakSigmaInit=0.36;}
	int binXmin = xySieveHist.GetXaxis()->FindBin(xSievePeak.mean - 3*xPeakSigmaInit);
	int binXmax = xySieveHist.GetXaxis()->FindBin(xSievePeak.mean + 3*xPeakSigmaInit);
	if(TMath::Abs(xSievePeak.mean - xComparison)<1.5){continue;}
	if(TMath::Abs(xSievePeak.mean - xComparison)>=1.5 && TMath::Abs(xSievePeak.mean - xComparison)<2.0){xPeakSigmaInit=0.35;}
	xComparison = xSievePeak.mean;
	
	//////////////////////////
	if (cmdOpts.automatic) {
	  //	  std::this_thread::sleep_for(std::chrono::milliseconds(cmdOpts.delay));
	}
	else {
	  cout << "    Continue? ";
	  cin >> tmp;
	}

	//////////////////////////
	double yComparison = -10.0;
	for (const auto& ySievePeak : ySievePeaksFit) {
	  tmpMark->SetY(ySievePeak.mean);
	  c1->cd();
	  tmpMark->Draw();
	  gPad->Update();
	  
	  if(TMath::Abs(ySievePeak.mean - yComparison)<0.95){continue;}
	  
	  double yPeakSigmaInit = ySievePeak.sigma;
	  if (yPeakSigmaInit>0.35){yPeakSigmaInit=0.35;}

	  // Find bounding box for current hole.
	  int binYmin = xySieveHist.GetYaxis()->FindBin(ySievePeak.mean - 3*yPeakSigmaInit);
	  int binYmax = xySieveHist.GetYaxis()->FindBin(ySievePeak.mean + 3*yPeakSigmaInit);
	  
	  // Want to have at least 50 events for fitting.
	  double integral = xySieveHist.Integral(
						 binXmin, binXmax,
						 binYmin, binYmax
						 );
	  
	  if (integral < 50) continue;
	  
	  // Fit x and y projection separately.
	  c2->cd();
	  tmpHist = xySieveHist.ProjectionX("_px", binYmin, binYmax);
	  tmpHist->GetXaxis()->SetRange(binXmin, binXmax);
	  tmpHist->Draw();
	  Peak xSievePeakSingle = fitPeak(
					  tmpHist,
					  xSievePeak.norm,
					  xSievePeak.mean,
					  xPeakSigmaInit
					  );
	  gPad->Update();
	  
	  c3->cd();
	  tmpHist = xySieveHist.ProjectionY("_py", binXmin, binXmax);
	  tmpHist->GetXaxis()->SetRange(binYmin, binYmax);
	  tmpHist->Draw();
	  Peak ySievePeakSingle = fitPeak(
					  tmpHist,
					  ySievePeak.norm,
					  ySievePeak.mean,
					  yPeakSigmaInit
					  );
	  gPad->Update();

	  int binYFitmin = xySieveHist.GetYaxis()->FindBin(ySievePeakSingle.mean - 2.2*ySievePeakSingle.sigma);
	  int binYFitmax = xySieveHist.GetYaxis()->FindBin(ySievePeakSingle.mean + 2.2*ySievePeakSingle.sigma);
	  int binXFitmin = xySieveHist.GetXaxis()->FindBin(xSievePeakSingle.mean - 2.2*xSievePeakSingle.sigma);
	  int binXFitmax = xySieveHist.GetXaxis()->FindBin(xSievePeakSingle.mean + 2.2*xSievePeakSingle.sigma);
	  integral = xySieveHist.Integral(
					  binXFitmin, binXFitmax,
					  binYFitmin, binYFitmax
					  );
	  if (integral<50){continue;}
	  ////////////////////////
	  if (cmdOpts.automatic) {
	    //	    std::this_thread::sleep_for(std::chrono::milliseconds(cmdOpts.delay));
	  }
	  else {
	    cout << "    Continue? ";
	    cin >> tmp;
	  }
	  /////////////////////////

	  // Construct bounding ellipse.
	  if (xSievePeakSingle.sigma!=0.0 && ySievePeakSingle.sigma!=0.0 && abs(xSievePeakSingle.mean)<15.0 && abs(ySievePeakSingle.mean)<10.0 && TMath::Abs(ySievePeakSingle.mean-yComparison)>0.95){
	    
	    TEllipse ellipse(
			     xSievePeakSingle.mean, ySievePeakSingle.mean,
			     2.2*xSievePeakSingle.sigma, 2*ySievePeakSingle.sigma
			     );
	    ellipse.SetLineColor(2);
	    ellipse.SetLineWidth(2);
	    ellipse.SetFillStyle(0);
	    
	    // Push everything to collection.
	    xSievePeaks.push_back(xSievePeakSingle);
	    ySievePeaks.push_back(ySievePeakSingle);
	    xSieveIndexes.push_back(getClosestIndex(xSievePeakSingle.mean, xSievePhys));
	    ySieveIndexes.push_back(getClosestIndex(ySievePeakSingle.mean, ySievePhys));
	    nEvents.push_back(0);
	    ellipses.push_back(ellipse);
	    yComparison = ySievePeakSingle.mean;
	    
	  }
	}
      }

      c1->cd();
      tmpMark->SetX(1000.0);
      tmpMark->Draw();
      for (auto& ellipse : ellipses) {
	xySieveHist.GetListOfFunctions()->Add(&ellipse);
      }
      gPad->Update();
      xySieveHist.Write();
      
      c2->Clear();
      gPad->Update();
      c3->Clear();
      gPad->Update();
      
      if (cmdOpts.automatic) {
	//	std::this_thread::sleep_for(std::chrono::milliseconds(cmdOpts.delay));
      }
      else {
	cout << "    Continue? ";
	cin >> tmp;
      }
      
      c1->Clear();
      gPad->Update();
    }  // foil loop
    
    // Cleanup of sieve fit.
    delete tmpHist;
    delete tmpMark;

    cout << "    Filling SVD matrices and vectors: ";
    iEvent = 0;

    reportProgressInit();
    for (const auto& event : events) {  // SVD filling loop
      if (iEvent%1000 == 0) reportProgress(iEvent, nEvents);
      ++iEvent;

      double cosTheta = cos(event.theta*TMath::DegToRad());
      double sinTheta = sin(event.theta*TMath::DegToRad());

      // Find which foil if any.
      uint iFoil = 0;
      for (iFoil=0; iFoil<nFoils; ++iFoil) {
	double zVerSigma = zVerPeaks.at(iFoil).sigma;
        if (
          zVerPeaks.at(iFoil).mean - 1.3*zVerSigma <= event.zVer &&
          event.zVer <= zVerPeaks.at(iFoil).mean + 1.3*zVerSigma &&
	  ((event.delta<1&&(yTarPeaks.at(nFoils-1-iFoil).mean - 1.0*yTarPeaks.at(nFoils-1-iFoil).sigma <= event.yTar &&
			    event.yTar <= yTarPeaks.at(nFoils-1-iFoil).mean + 1.0*yTarPeaks.at(nFoils-1-iFoil).sigma))||
	   (event.delta>=1&&(yTarPeaks.at(nFoils-1-iFoil).mean - 1.8*yTarPeaks.at(nFoils-1-iFoil).sigma <= event.yTar &&
			     event.yTar <= yTarPeaks.at(nFoils-1-iFoil).mean + 1.8*yTarPeaks.at(nFoils-1-iFoil).sigma)))
	  &&event.delta>-12
        ) {
          break;
        }
      }
      //if (iFoil!=0) continue;/////this is only to test!!!!!!!!!!!!!!!!!!!!!!
      // Skip event if it is too far from any foil.
      if (iFoil == nFoils) continue;

      // Find which sieve hole if any for corresponding delta. 
      uint iHole = 0;
      
      for (iHole=0; iHole<xSievePeakss.at(iFoil).size(); ++iHole) {
	Peak& xSieveP = xSievePeakss.at(iFoil).at(iHole);
	Peak& ySieveP = ySievePeakss.at(iFoil).at(iHole);
	if (
	    xSieveP.mean - 2.2*xSieveP.sigma <= event.xSieve &&
	    event.xSieve <= xSieveP.mean + 2.2*xSieveP.sigma &&
	    ySieveP.mean - 2*ySieveP.sigma <= event.ySieve &&
	    event.ySieve <= ySieveP.mean + 2*ySieveP.sigma
	    ) {
	  break;
	}
      }

      // Skip event if it is too far from any hole or if there is enough events
      // from this hole already.
      if (
	  iHole == xSievePeakss.at(iFoil).size() ||
	  nEventss.at(iFoil).at(iHole) > 50
	  ) {
	continue;
      }
      ++nEventss.at(iFoil).at(iHole);

      // Calculate the real or "physical" event quantities.
      double zFoil = runConf.zFoils.at(iFoil);

      double xTarVerPhy = -event.yVer- runConf.SHMS.xMispointing;
      double yTarVerPhy = -zFoil*sinTheta + event.xVer*cosTheta - runConf.SHMS.yMispointing;
      double zTarVerPhy = zFoil*cosTheta + event.xVer*sinTheta;

      double xpTarPhy =
        (xSievePhys.at(xSieveIndexess.at(iFoil).at(iHole)) - xTarVerPhy) /
        (runConf.sieve.z0 - zTarVerPhy);
      
      double Cdelta = -0.019*event.delta+0.00019*pow(event.delta,2) + 40.0*(-0.00052*event.delta+0.0000052*pow(event.delta,2));
      double ypTarPhy =
	(ySievePhys.at(ySieveIndexess.at(iFoil).at(iHole)) - Cdelta - yTarVerPhy) /
        (runConf.sieve.z0 - zTarVerPhy);
      
      double xTarPhy = xTarVerPhy - xpTarPhy*zTarVerPhy; 
      double yTarPhy = yTarVerPhy - ypTarPhy*zTarVerPhy; 


      //h2_yTarVdeltaReal->Fill(yTarPhy, event.delta);

      h2_xpTar->Fill(xpTarPhy,event.xpTar-xpTarPhy);
      h2_ypTar->Fill(ypTarPhy,event.ypTar-ypTarPhy);
      h2_yTar->Fill(yTarPhy, event.yTar-yTarPhy);
      h2_zVer->Fill(zFoil,event.zVer - zFoil);
      h2_xSieveAng[iFoil]->Fill(xSievePhys.at(xSieveIndexess.at(iFoil).at(iHole)),event.xpTar-xpTarPhy);
      h2_ySieveAng[iFoil]->Fill(ySievePhys.at(ySieveIndexess.at(iFoil).at(iHole)),event.ypTar-ypTarPhy);

      h_xptar_xsieve[iFoil][xSieveIndexess.at(iFoil).at(iHole)]->Fill(event.xpTar-xpTarPhy);  
      h_yptar_ysieve[iFoil][ySieveIndexess.at(iFoil).at(iHole)]->Fill(event.ypTar-ypTarPhy);
      h_ytar_ysieve[iFoil][ySieveIndexess.at(iFoil).at(iHole)]->Fill(event.yTar-yTarPhy);
    

      // Calculate contributions of xTar dependent terms.
      // Use old reconstruction matrix and xTarPhy.
      xpSumDep = 0.0;
      ySumDep = 0.0;
      ypSumDep = 0.0;

      for (const auto& line : recMatrixDep.matrix) {
        lambda =
          pow(event.xFp/100.0, line.E_x) *
          pow(event.xpFp, line.E_xp) *
          pow(event.yFp/100.0, line.E_y) *
          pow(event.ypFp, line.E_yp) *
          pow(xTarPhy/100.0, line.E_xTar);

        xpSumDep += line.C_Xp * lambda;
        ySumDep += line.C_Y * lambda;
        ypSumDep += line.C_Yp * lambda;
      }

      // Calculate lambdas for xTar independent terms.
      // Use new matrix and xTarPhy.
      std::vector<double> lambdas;
      for (const auto& line : recMatrixNew.matrix) {
        lambdas.push_back(
          pow(event.xFp/100.0, line.E_x) *
          pow(event.xpFp, line.E_xp) *
          pow(event.yFp/100.0, line.E_y) *
          pow(event.ypFp, line.E_yp) *
          pow(xTarPhy/100.0, line.E_xTar)
        );
      }

      // Add lambda_i * lambda_j to (i,j)-th element of SVD matrices.
      // Add (_TarPhy - _SumDep) to SVD vectors.
      // We only have xTar independent terms.
      Int_t i = 0;
      Int_t j = 0;
      for (const auto& lambda_i : lambdas) {
        j = 0;
        for (const auto& lambda_j : lambdas) {
          xpTarFitMat(i, j) += lambda_i * lambda_j;
          yTarFitMat(i, j) += lambda_i * lambda_j;
          ypTarFitMat(i, j) += lambda_i * lambda_j;

          ++j;
        }

        xpTarFitVec(i) += lambda_i * (xpTarPhy - xpSumDep);
        yTarFitVec(i) += lambda_i * (yTarPhy/100.0 - ySumDep);
        ypTarFitVec(i) += lambda_i * (ypTarPhy - ypSumDep);

        ++i;
      }
    }  // SVD filling loop


    double xptarDiff[nFoils][ixSieve];
    double yptarDiff[nFoils][iySieve];
    double ytarDiff[nFoils][iySieve];
    TGraph *g1[nFoils];
    TGraph *g2[nFoils];
    TGraph *g3[nFoils];

    double xSievePhysFormat[nFoils][ixSieve];
    double ySievePhysFormat[nFoils][iySieve];


    for (uint iFoil=0; iFoil<nFoils; iFoil++){

      for (uint ii=0; ii<ixSieve; ii++){
        xSievePhysFormat[iFoil][ii] = runConf.sieve.xHoleMin + ii*runConf.sieve.xHoleSpace;
	h_xptar_xsieve[iFoil][ii]->Fit("gaus","Q");
        h_xptar_xsieve[iFoil][ii]->Draw();
        h_xptar_xsieve[iFoil][ii]->Write();
        if (h_xptar_xsieve[iFoil][ii]->Integral()>0.0){
          xptarDiff[iFoil][ii] = h_xptar_xsieve[iFoil][ii]->GetFunction("gaus")->GetParameter(1);
        }
        else{xptarDiff[iFoil][ii] = 0.0;}
      }

      for (uint ii=0; ii<iySieve; ii++){
	if (runConf.sievetype>1){
	  ySievePhysFormat[iFoil][ii] = runConf.sieve.yHoleMin+runConf.sieve.yHoleSpace/2.0+ii*runConf.sieve.yHoleSpace;
	}
	else{
	  ySievePhysFormat[iFoil][ii] = runConf.sieve.yHoleMin + ii*runConf.sieve.yHoleSpace;
	}
	h_ytar_ysieve[iFoil][ii]->Fit("gaus","Q");
        h_ytar_ysieve[iFoil][ii]->Draw();
        if (h_ytar_ysieve[iFoil][ii]->Integral()>0.0){
	  ytarDiff[iFoil][ii] = h_ytar_ysieve[iFoil][ii]->GetFunction("gaus")->GetParameter(1);
	}
	else{ytarDiff[iFoil][ii] = 0.0;}
	h_yptar_ysieve[iFoil][ii]->Fit("gaus","Q");
        h_yptar_ysieve[iFoil][ii]->Draw();
        if (h_yptar_ysieve[iFoil][ii]->Integral()>0.0){
	  yptarDiff[iFoil][ii]= h_yptar_ysieve[iFoil][ii]->GetFunction("gaus")->GetParameter(1);
	}
	else{yptarDiff[iFoil][ii] = 0.0;}	
      }
           
      g1[iFoil] = new TGraph(ixSieve, xSievePhysFormat[iFoil],xptarDiff[iFoil]);
      g2[iFoil] = new TGraph(iySieve, ySievePhysFormat[iFoil], yptarDiff[iFoil]);
      g3[iFoil] = new TGraph(iySieve, ySievePhysFormat[iFoil], ytarDiff[iFoil]);
      
      g1[iFoil]->SetMarkerColor(kBlue);
      g1[iFoil]->SetMarkerStyle(21);
      g1[iFoil]->SetTitle(Form("Foil %d", iFoil));
      g1[iFoil]->GetXaxis()->SetTitle("xSieve");
      g1[iFoil]->GetYaxis()->SetTitle("xpTar_{m}-xpTar_{real}");      
      g2[iFoil]->SetMarkerColor(kBlue);
      g2[iFoil]->SetMarkerStyle(21);
      g3[iFoil]->SetMarkerColor(kBlue);
      g3[iFoil]->SetMarkerStyle(21);
      g2[iFoil]->SetTitle(Form("Foil %d", iFoil));
      g2[iFoil]->GetXaxis()->SetTitle("ySieve");
      g2[iFoil]->GetYaxis()->SetTitle("ypTar_{m}-ypTar_{real}");
      g3[iFoil]->SetTitle(Form("Foil %d", iFoil));
      g3[iFoil]->GetXaxis()->SetTitle("ySieve");
      g3[iFoil]->GetYaxis()->SetTitle("yTar_{m}-yTar_{real}");
      
      g1[iFoil]->Draw("AP");
      g2[iFoil]->Draw("AP");
      g3[iFoil]->Draw("AP");
      
      g1[iFoil]->Write();
      g2[iFoil]->Write();
      g3[iFoil]->Write();
    }

    h2_xpTar->Draw();
    h2_ypTar->Draw();
    h2_yTar->Draw();
    h2_zVer->Draw();
    h2_yTarVypTar->Draw();
    h2_yTarVdelta->Draw();
    h2_yTarVdelta_cut->Draw();
    h2_fp->Draw();
    
    h2_xpTar->Write();
    h2_ypTar->Write();
    h2_yTar->Write();
    h2_zVer->Write();
    h2_yTarVypTar->Write();
    h2_yTarVdelta->Write();
    h2_yTarVdelta_cut->Write();
    h2_fp->Write();

    reportProgressFinish(); 

  }  // run loop


  std::ofstream ofs("xpVec.txt");
  std::ios::fmtflags f1(ofs.flags());
  std::streamsize prevPrec1 = ofs.precision(9);
  for (Int_t iTerm=0; iTerm<recMatrixNewLen; ++iTerm) {
    ofs << std::scientific << std::setw(17) << xpTarFitVec(iTerm) << endl;
  }
  ofs.precision(prevPrec1);
  ofs.flags(f1);
  ofs.close();

  ofs.open("xpMat.txt");
  std::ios::fmtflags f2(ofs.flags());
  std::streamsize prevPrec2 = ofs.precision(9);
  for (Int_t iTerm=0; iTerm<recMatrixNewLen; ++iTerm) {
    for (Int_t jTerm=0; jTerm<recMatrixNewLen; ++jTerm) {
      ofs
        << std::scientific << std::setw(17)
        << xpTarFitMat(iTerm, jTerm);
    }
    ofs << endl;
  }
  ofs.precision(prevPrec2);
  ofs.flags(f2);
  ofs.close();


  cout << "Solving SVD problems:" << endl;
  TDecompSVD xpTarSVD(xpTarFitMat);
  TDecompSVD yTarSVD(yTarFitMat);
  TDecompSVD ypTarSVD(ypTarFitMat);

  bool xpTarSuccess = xpTarSVD.Solve(xpTarFitVec);
  cout << "  xpTar: " << (xpTarSuccess ? "success" : "failure") << endl;
  bool yTarSuccess = yTarSVD.Solve(yTarFitVec);
  cout << "  yTar: " << (yTarSuccess ? "success" : "failure") << endl;
  bool ypTarSuccess = ypTarSVD.Solve(ypTarFitVec);
  cout << "  ypTar: " << (ypTarSuccess ? "success" : "failure") << endl;


  cout << "Constructing new xTar independent optics matrix." << endl;
  Int_t iTerm = 0;
  for(auto& line : recMatrixNew.matrix) {
    line.C_Xp = xpTarFitVec(iTerm);
    line.C_Y = yTarFitVec(iTerm);
    line.C_Yp = ypTarFitVec(iTerm);

    ++iTerm;
  }

  recMatrixDepFileName = conf.recMatrixFileNameNew;
  recMatrixDepFileName.insert(
    conf.recMatrixFileNameNew.size()-4, "__dep"
  );
  recMatrixIndepFileName = conf.recMatrixFileNameNew;
  recMatrixIndepFileName.insert(
    conf.recMatrixFileNameNew.size()-4, "__indep"
  );

  cout
    << "Saving xTar independent matrix to:" << endl
    << "  `" << recMatrixIndepFileName << "`" << endl;
  writeMatrixFile(recMatrixIndepFileName, recMatrixNew);
  cout
    << "Saving xTar dependent matrix to:" << endl
    << "  `" << recMatrixDepFileName << "`" << endl;
  writeMatrixFile(recMatrixDepFileName, recMatrixDep);

  // Cleanup and exit.
  delete c3;
  delete c2;
  delete c1;

 

  return 0;
}
