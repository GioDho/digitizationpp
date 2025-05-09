//CALL: ./progname.exe Configfile -I inputdir -O outputdir

#include <iostream>
#include <unistd.h>
#include <limits.h>
#include <sstream>
#include <fstream>
#include <chrono>
#include <map>
#include <random>
#include <string>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <stdio.h>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include "TRandom3.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1.h"
#include "TH2.h"
#include "s3.h"
#include "cygnolib.h"
#include <oneapi/tbb.h>
#include <oneapi/tbb/info.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/task_arena.h>
#include <oneapi/tbb/mutex.h>
#include <oneapi/tbb/enumerable_thread_specific.h>
#include <thread>

using namespace std;
using namespace oneapi;


// Global and constant value;
double GEM1_gain;
double GEM2_gain;
double GEM3_gain;
double extraction_eff_GEM1;
double extraction_eff_GEM2;
double extraction_eff_GEM3;
double omega;
int x_ini = 0;
int y_ini = 0;
int z_ini = 0;
int x_pix;
int y_pix;
double optcounts_per_photon;
double y_sensor_size;
double readout_time;
int num_threads;



vector<string> split_str(const string& , char );
void ReadConfig(string name, map<string,string>& options);
void ReadG4Isotopes(string name, map<string,string>& dict_isotopes);
void ReadIonlist(string name, vector<vector<string>>& ionlist);
void SaveValues(map<string,string>& options, shared_ptr<TFile>& outfile);

vector<double> NelGEM1(const vector<double>& N_ioniz_el);
vector<double> NelGEM2(const vector<double>& energyDep,const vector<double>& z_hit, map<string,string>& options);

void AddBckg(map<string,string>& options, vector<vector<int>>& background);

bool is_NR(vector<int> pdgID_hits, int pdg);
double angle_between(vector<double>& v1, vector<double>& v2);
vector<double> crossProduct(vector<double>& a, vector<double>& b);
vector<double> rotateByAngleAndAxis(vector<double>& vec, double angle, vector<double>& axis);

void compute_cmos_with_saturation(vector<double>& x_hits_tr,
                                  vector<double>& y_hits_tr,
                                  vector<double>& z_hits_tr,
                                  vector<double>& energy_hits,
                                  map<string,string>& options,
                                  vector<vector<double>>& array2d_Nph,
                                  Float_t energy,
                                  bool NR_flag);

void compute_cmos_without_saturation(vector<double>& x_hits_tr,
                                     vector<double>& y_hits_tr,
                                     vector<double>& z_hits_tr,
                                     vector<double>& energy_hits,
                                     map<string,string>& options,
                                     vector<vector<double>>& array2d_Nph);

void cloud_smearing3D(vector<double>& x_hits_tr,
                      vector<double>& y_hits_tr,
                      vector<double>& z_hits_tr,
                      vector<double>& energy_hits,
                      map<string,string>& options,
                      vector<float>& S3D_x,
                      vector<float>& S3D_y,
                      vector<float>& S3D_z);

void ph_smearing2D(vector<double>& x_hits_tr,
                   vector<double>& y_hits_tr,
                   vector<double>& z_hits_tr,
                   vector<double>& energy_hits,
                   map<string,string>& options,
                   vector<float>& S2D_x,
                   vector<float>& S2D_y) ;

vector<double> compute_sigma(const double diff_const, const double diff_coeff, const vector<double>& dz);
vector<float> smear(const vector<double>& axis_hit, const vector<double>& axis_sigma, const vector<double>& nel);
void smear_parallel(const vector<double>& x_hits_tr,
                    const vector<double>& y_hits_tr,
                    const vector<double>& z_hits_tr,
                    const vector<double>& sigma_x,
                    const vector<double>& sigma_y,
                    const vector<double>& sigma_z,
                    const vector<double>& nel,
                    vector<float>& S3D_x,
                    vector<float>& S3D_y,
                    vector<float>& S3D_z);

vector<double> arange(double start, double stop, double step);
double round_up_to_even(const double f);

//void Nph_saturation(const TH3I& h3d, map<string,string>& options, vector<vector<double>>& hout);
double Nph_saturation(int nel, double A, double beta);

void TrackVignetting(vector<vector<double>>& array2d_Nph, int xpix, int ypix, const TH2F & VignMap);

// Old approach
//string rootlocation(string tag, int run);   // inconsistency for the MC-old tag!!!
//string root_filename(string tag, int run);

int main(int argc, char** argv)
{
    string outfolder;
    string infolder;
   
    if(argc<2) {cerr<<"No Configfile given!!\nSuggested use: ./progname.exe Configfile -I inputdir -O outputdir"; exit(EXIT_FAILURE);}
    string nome=argv[1];
    
    vector<string> path_to_config=split_str(nome,'/');
    string parte= "";
    for(unsigned int i=0;i<path_to_config.size()-2;i++) parte=parte+path_to_config[i]+ "/";
    char buffer[PATH_MAX];
    getcwd(buffer,sizeof(buffer));
    string currentPath(buffer);
    const string SOURCE_DIR= currentPath+"/"+parte;
   
    if(argc<3)
    {
        infolder=SOURCE_DIR+"/input/";
        outfolder=SOURCE_DIR+"/OutDir/";
    }
    else
    {
        if(argc!=6) {cerr<<"Wrong parameter inputs given!!\nCorrect use: ./progname.exe Configfile -I inputdir -O outputdir\n"; exit(EXIT_FAILURE);}
        string parseop=argv[2];
        string parseop2=argv[4];
        //cout<< parseop << "   " << parseop2<< endl;
        if(parseop!="-I" && parseop2!="-I") {cerr<<"Wrong options name!!\nSuggested use: ./progname.exe Configfile -I inputdir -O outputdir\n"; exit(EXIT_FAILURE);}
        if(parseop!="-O" && parseop2!="-O") {cerr<<"Wrong options name!!\nSuggested use: ./progname.exe Configfile -I inputdir -O outputdir\n"; exit(EXIT_FAILURE);}
	
        if(parseop=="-I") {infolder=argv[3]; outfolder=argv[5];}
        else {infolder=argv[5]; outfolder=argv[3];}
    }

    // DEBUG
    // cout<<"Input Folder: "<<infolder<<endl;
    // cout<<"Output Folder: "<<outfolder<<endl;
    
    map<string,string> options;
    ReadConfig(nome,options);						//Function to be checked
	
    GEM1_gain = 0.0347*exp((0.0209)*stod(options["GEM1_HV"]));
    GEM2_gain = 0.0347*exp((0.0209)*stod(options["GEM2_HV"]));
    GEM3_gain = 0.0347*exp((0.0209)*stod(options["GEM3_HV"]));
    cout<< "GEM1_gain = " << GEM1_gain;
    cout<< "\nGEM2_gain = " << GEM2_gain;
    cout<< "\nGEM3_gain = " << GEM3_gain << endl;
    
    //dividing Fernando's to Francesco&Karolina's single GEM gain measurement
    extraction_eff_GEM1 = 0.87319885*exp(-0.0020000000*stod(options["GEM1_HV"]));
    extraction_eff_GEM2 = 0.87319885*exp(-0.0020000000*stod(options["GEM2_HV"]));
    extraction_eff_GEM3 = 0.87319885*exp(-0.0020000000*stod(options["GEM3_HV"]));
    cout<< "extraction eff GEM1 = " << extraction_eff_GEM1;
    cout<< "\nextraction eff GEM2 = " << extraction_eff_GEM2;
    cout<< "\nextraction eff GEM3 = " << extraction_eff_GEM3 << endl;

    y_pix = 2304;
    if(options["Camera_type"]== "Fusion")
    {
        x_pix = 2304;
        optcounts_per_photon = 2.;       //apparently calibrated with LEMOn in the past
        y_sensor_size = 14.976;         //mm
        readout_time = 184.4;           //ms in in ultra quiet scan (UQS)
    }
    else if(options["Camera_type"]== "Quest1")
    {
        x_pix = 4096;
        optcounts_per_photon = 4.49;     // equal to 2.*2.245 which is the ratio of e/count of the two cameras (fusion and quest)
        y_sensor_size = 10.598;          //mm
        readout_time = 199.;               // ms in UQS
    }
    else if(options["Camera_type"]== "Quest2")
    {
        x_pix = 4096;
        optcounts_per_photon = 4.49;
        y_sensor_size = 10.598;         //ms
        readout_time = 39.;               // ms in UQS
    }

    double y_dim=stod(options["y_dim"]);
    double x_dim=stod(options["x_dim"]);
    double demag=y_dim/y_sensor_size;
    double aperture=stod(options["camera_aperture"]);
    omega=1./pow(4.*(demag+1)*aperture,2);

    //Check if dimension imaged and pixel ratio matches
    double testvalue = (x_dim/y_dim) - (static_cast<double>(x_pix)/static_cast<double>(y_pix));
    if(testvalue>1e-1)
    {
        cerr<<"You are using a Quest image dimension with a Fusion simulation! Digitization FAILED!\n";
        exit(0);
    }
    else if(testvalue<-1e-1)
    {
            cerr<<"You are using a Fusion image dimension with a Quest simulation! Digitization FAILED!\n";
            exit(0);
    }


    num_threads=stoi(options["Parallel_threads"]);  //use tbb::info::default_concurrency(); to get all of the available cores
	
    //Code execution
    int run_count = stoi(options["start_run_number"]);
    auto t0 = std::chrono::steady_clock::now();
    
    if(options["fixed_seed"]=="True" || options["fixed_seed"]=="true") gRandom->SetSeed(10);
    
    vector<int> eventnumber;
    vector<int> particle_type;
    vector<float> energy_ini;
    vector<float> theta_ini;
    vector<float> phi_ini;
    
    if(! filesystem::exists(outfolder) && options["queue"]=="0"){
        //DEBUG
        cout<<"Creating oufolder..."<<
        system(("mkdir -p " + outfolder).c_str() );
    }
    
    map<string, string> dict_isotopes;
    if(options["GEANT4isotopes"]=="True") ReadG4Isotopes("../Z_A_isotopes.csv", dict_isotopes);
    
    
    
    string ending=".root";
    for(const auto& fentry : filesystem::directory_iterator(infolder))
    {
        string filename=fentry.path();
        cout<<filename<<endl;
        bool ends= false;
        if (ending.size() <= filename.size())    ends=equal(ending.rbegin(), ending.rend(), filename.rbegin());
        
        //DEBUG
        if(!ends) cout<<"Not ending with "<<ending<<endl;
        else cout<<filename<<" is ending with "<<ending<<"!"<<endl;
        
        //if(filename.ends_with(".root"))   //c++20 fix it wtf
        //{
        
        if(ends) {
            
            //DEBUG
            if(filename.find("HeCF4gas_AmBe_part") != string::npos) {
            //if(filename.find("LIME_CADshield") != string::npos) {
                continue;
            }
            
            vector<vector<string>> SRIM_events;
            
            if(options["NR"]=="True" && options["NR_list"]!="") {
                auto delim1 = filename.find("part");
                auto delim2 = filename.find(".root");
                if(delim1==string::npos) throw runtime_error("Cannot determine the 'part' of the file.\n");
                if(delim2==string::npos) throw runtime_error("Input file is not a root file.\n");
                auto part= filename.substr(delim1+4, delim2-delim1-4);
                cout<<"Using NR list from "<<options["NR_list"]<<"_part"<<part<<".py"<<endl;
                
                ReadIonlist(Form("%s/%s_part%s.py", infolder.c_str(), options["NR_list"].c_str(), part.c_str()),
                            SRIM_events);
                
            }
            
            
            
            
            auto f         = unique_ptr<TFile> {TFile::Open(filename.c_str())};
            auto inputtree = (TTree*)f->Get("nTuple");
            
            int max_events = inputtree->GetEntries();
            int totev = (stod(options["events"])==-1) ? max_events : stod(options["events"]);
            totev = min(totev, max_events);
            
            int firstentry = stoi(options["start_event"]);
            if (firstentry>totev) throw runtime_error("Error: First entry is larger than last entry, exiting!");
            cout << "Processing entries from "<<firstentry<<" to "<<totev<<"."<<endl;
            
            
            // output saved in outfolder/filename/
            
            stringstream ssfilename(filename.c_str());
            string ssfilename_el, filename_tmp;
            while(getline(ssfilename, ssfilename_el, '/')) {
                filename_tmp = ssfilename_el;
            }
            auto delimFN = filename_tmp.find(".root");
            string basefilename   = filename_tmp.substr(0, delimFN);
            string fnameoutfolder = outfolder + "/" + basefilename;
            if(! filesystem::exists(fnameoutfolder) && options["queue"]=="0"){
                system(("mkdir -p " + fnameoutfolder).c_str() );
            }
            
            // standard: name of output file = histograms_RunRRRRR.root (R run number)
            if(options["queue"]=="1") fnameoutfolder="./";
            string fileoutname= Form("%s/histograms_Run%05d.root",
                                     fnameoutfolder.c_str(),
                                     run_count);
            
            // for radioisotope simulation: histograms_RunZZAAANN.root (Z=atomic number, A=mass numbe$
            // NOTE: this is a 7 digit run number, while reconstruction currently looks for 5
            string isot_numb = "0000000";
            if(options["GEANT4isotopes"]=="True") {
                cout<<"GEANT4isotopes option is active."<<endl;
                
                stringstream ssinfile(basefilename);
                string tmpstr;
                int counter = 0;
                while(getline(ssinfile, tmpstr, '_')) {
                    if(counter == 1) {
                        isot_numb = dict_isotopes[tmpstr];
                        // DEBUG
                        //isot_numb = "00000";
                    }
                    counter++;
                }
                
                auto delimBFN = basefilename.find("part");
                if(delimBFN==string::npos) throw runtime_error("Cannot determine the 'part' of the file.\n");
                auto part = basefilename.substr(delimBFN+4, basefilename.size() - delimBFN - 4);
                      
                if(filename.find("part")!= string::npos) {
                    fileoutname = Form("%s/histograms_Run%05d%02d.root",
                                       fnameoutfolder.c_str(),
                                       stoi(isot_numb),
                                       stoi(part)
                                       );
                    isot_numb = Form("%05d%02d", stoi(isot_numb), stoi(part));
                } else {
                    fileoutname = Form("%s/histograms_Run%05d00.root",
                                       fnameoutfolder.c_str(),
                                       stoi(isot_numb));
                    
                    isot_numb = Form("%05d00", stoi(isot_numb));
                }
                //DEBUG
                //cout<<"DEBUG: fileoutname = "<<fileoutname<<endl;
                //cout<<"DEBUG: isot_numb = "<<isot_numb<<endl;
            }
            
            if(options["start_event"]!="0"){
                cout<<"out folder "<<fnameoutfolder<<endl;
                int newpart = (int)(stoi(options["start_event"])/500);
                int oldpart = stoi(isot_numb);
                int partnum = oldpart + newpart;
                fileoutname = Form("%s/histograms_Run%05d.root",
                                   fnameoutfolder.c_str(),
                                   partnum);
            }
                        
            
            auto outfile = shared_ptr<TFile> {TFile::Open(fileoutname.c_str(),
                                                          "RECREATE") };
            //outfile->mkdir("event_info");
            SaveValues(options,outfile);
            
            //Output file branches
            Int_t eventnumber_out   = -999;
            Int_t particle_type_out = -999;
            Float_t energy = -999;
            Float_t theta  = -999;
            Float_t phi    = -999;
            Float_t track_length_3D = -1;
            Float_t x_vertex = -1;
            Float_t y_vertex = -1;
            Float_t z_vertex = -1;
            Float_t x_vertex_end = -1;
            Float_t y_vertex_end = -1;
            Float_t z_vertex_end = -1;
            Float_t x_min = -1;
            Float_t x_max = -1;
            Float_t y_min = -1;
            Float_t y_max = -1;
            Float_t z_min = -1;
            Float_t z_max = -1;
            Float_t px = -1;
            Float_t py = -1;
            Float_t pz = -1;
            Float_t proj_track_2D = -1;
            Int_t nhits_og = -1;
            Int_t N_photons = -1;
            
            Int_t row_cut = -1;
            Int_t N_photons_cut = -1;
            Float_t cut_energy = -1;
            Float_t x_min_cut = -1;
            Float_t x_max_cut = -1;
            Float_t y_min_cut = -1;
            Float_t y_max_cut = -1;
            Float_t z_min_cut = -1;
            Float_t z_max_cut = -1;
            Float_t proj_track_2D_cut = -1;
            
            auto outtree = new TTree("event_info", "event_info");
            
            outtree->Branch("eventnumber", &eventnumber_out, "eventnumber/I");
            outtree->Branch("particle_type", &particle_type_out, "particle_type/I");
            outtree->Branch("energy", &energy, "energy/F");
            outtree->Branch("theta", &theta, "theta/F");
            outtree->Branch("phi", &phi, "phi/F");
            outtree->Branch("track_length_3D", &track_length_3D, "track_length_3D/F");
            outtree->Branch("proj_track_2D", &proj_track_2D, "proj_track_2D/F");
            outtree->Branch("x_vertex", &x_vertex, "x_vertex/F");
            outtree->Branch("y_vertex", &y_vertex, "y_vertex/F");
            outtree->Branch("z_vertex", &z_vertex, "z_vertex/F");
            outtree->Branch("x_vertex_end", &x_vertex_end, "x_vertex_end/F");
            outtree->Branch("y_vertex_end", &y_vertex_end, "y_vertex_end/F");
            outtree->Branch("z_vertex_end", &z_vertex_end, "z_vertex_end/F");
            outtree->Branch("x_min", &x_min, "x_min/F");
            outtree->Branch("x_max", &x_max, "x_max/F");
            outtree->Branch("y_min", &y_min, "y_min/F");
            outtree->Branch("y_max", &y_max, "y_max/F");
            outtree->Branch("z_min", &z_min, "z_min/F");
            outtree->Branch("z_max", &z_max, "z_max/F");
            outtree->Branch("N_photons", &N_photons, "N_photons/I");
            outtree->Branch("px", &px, "px/F");
            outtree->Branch("py", &py, "py/F");
            outtree->Branch("pz", &pz, "pz/F");
            outtree->Branch("nhits_og", &nhits_og, "nhits_og/I");
            if (options["exposure_time_effect"]=="True") {
                outtree->Branch("N_photons_cut", &N_photons_cut, "N_photons_cut/I");
                outtree->Branch("row_cut", &row_cut, "row_cut/I");
                outtree->Branch("cut_energy", &cut_energy, "cut_energy/F");
                outtree->Branch("x_min_cut", &x_min_cut, "x_min_cut/F");
                outtree->Branch("x_max_cut", &x_max_cut, "x_max_cut/F");
                outtree->Branch("y_min_cut", &y_min_cut, "y_min_cut/F");
                outtree->Branch("y_max_cut", &y_max_cut, "y_max_cut/F");
                outtree->Branch("z_min_cut", &z_min_cut, "z_min_cut/F");
                outtree->Branch("z_max_cut", &z_max_cut, "z_max_cut/F");
                outtree->Branch("proj_track_2D_cut", &proj_track_2D_cut, "proj_track_2D_cut/F");
            }
            
            // Input file branches
            Int_t eventnumber;
            Int_t numhits;
            Double_t energyDep;
            Double_t energyDep_NR;
            Float_t  ekin_particle;
            Int_t particle_type;
            vector<int>    *pdgID_hits = 0;
            vector<double> *tracklen_hits = 0;
            vector<double> *px_particle = 0;
            vector<double> *py_particle = 0;
            vector<double> *pz_particle = 0;
            vector<double> *energyDep_hits = 0;
            vector<double> *x_hits = 0;
            vector<double> *y_hits = 0;
            vector<double> *z_hits = 0;
            
            
            inputtree->SetBranchAddress("eventnumber", &eventnumber);
            inputtree->SetBranchAddress("numhits", &numhits);
            
            if(options["SRIM"]=="False") { // TO BE CHECK ON REAL SRIM SIMULATIONS
                inputtree->SetBranchAddress("energyDep",    &energyDep);
                inputtree->SetBranchAddress("energyDep_NR", &energyDep_NR);
                inputtree->SetBranchAddress("pdgID_hits",    &pdgID_hits);
                inputtree->SetBranchAddress("tracklen_hits", &tracklen_hits);
                inputtree->SetBranchAddress("px_particle", &px_particle);
                inputtree->SetBranchAddress("py_particle", &py_particle);
                inputtree->SetBranchAddress("pz_particle", &pz_particle);
            }
            
            inputtree->SetBranchAddress("energyDep_hits", &energyDep_hits);
            inputtree->SetBranchAddress("x_hits", &x_hits);
            inputtree->SetBranchAddress("y_hits", &y_hits);
            inputtree->SetBranchAddress("z_hits", &z_hits);
            
            if(options["NR"]=="True") { // TO BE CHECK ON REAL SRIM SIMULATIONS
                inputtree->SetBranchAddress("particle_type", &particle_type);
                inputtree->SetBranchAddress("ekin_particle", &ekin_particle);
            }

            TH2F VignMap;
            if(options["Vignetting"]=="True") {
                string vignfilename = Form("%sVignettingMap/%s",SOURCE_DIR.c_str(), options["Vig_Map"].c_str());
                cout<<"Opening "<<vignfilename<<"..."<<endl;
                auto VignFile = unique_ptr<TFile> {TFile::Open(vignfilename.c_str())};
                
                VignMap = (*(TH2F*)VignFile->Get("normmap"));
                
                VignMap.Smooth();
                
                VignFile->Close();

                int x_vign=VignMap.GetNbinsX();
                int y_vign=VignMap.GetNbinsY();

                if(x_vign!=y_vign)
                {
                    if(x_pix==y_pix)
                    {
                        cerr<<"You are using a Quest vignette map with a Fusion simulation! Digitization FAILED!\n";
                        exit(0);
                    }
                }
                else
                {
                    if(x_pix!=y_pix)
                    {
                        cerr<<"You are using a Fusion vignette map with a Quest simulation! Digitization FAILED!\n";
                        exit(0);
                    }

                }
            }
            
            //DEBUG
            //cout<<"DEBUG: "<<VignMap.GetBinContent(100,100)<<endl;
            
            for(int entry=0; entry<totev; entry++) {  // RUNNING ON ENTRIES
                
                inputtree->GetEntry(entry);
                
                //DEBUG
                //if(options["NR"]=="True") cout<<particle_type<<endl;
                //if (entry==0) {
                //    cout<<numhits<<" - "<<pdgID_hits->size()<<endl;
                //    for(unsigned int i=0; i<pdgID_hits->size(); i++) {
                //        cout<<"---"<< (*pdgID_hits)[i] <<endl;
                //    }
                //}
                
                cout<<"\nEntry "<<entry<<" of "<<totev<<endl;
                
                if (options["SRIM"]=="True"){
                    cout<<"Energy "<<ekin_particle<<" keV"<<endl;
                } else {
                    cout<<"Energy "<<energyDep    <<" keV"<<endl;
                }

                bool NR_flag=false;
                if(options["SRIM"] == "True") {
                    energy            = ekin_particle;
                    particle_type_out = particle_type;
                    //if(particle_type_out==??) NR_flag=true;       Not known output from SRIM
                } else {
                    // this would be the energy of the primary particle - equal to
                    // deposited energy only if it is completely contained in the sensitive volume
                    // energy = ekin_particle * 1000;
                    energy = energyDep;
                    if (energyDep_NR>0){
                        particle_type_out = is_NR(*pdgID_hits, int(1.e9));
                        NR_flag = true;
                    } 
                    else particle_type_out = (*pdgID_hits)[0];  // this will tell us if the deposit was
                    // started by a photon or an electron
                }

                if(options["NR"]=="False" && NR_flag==true ) continue;
                if(options["SRIM"]=="True"  && ekin_particle>900) continue;     //not corrected for SRIM
                
                //initialize array values - to save info also if the track is skipped (background only)
                row_cut         = -1;
                eventnumber_out = eventnumber;
                
                
                // DEBUG
                //cout<<"energyDep_NR = "<<energyDep_NR<<endl;
                //cout<<"particle_type_out = "<<particle_type_out<<endl;
                
                //particle_type_out = -999;
                //energy = -1;
                cut_energy      = -1;
                theta           =  0;
                phi             =  0;
                track_length_3D = -1;
                proj_track_2D   = -1;
                x_vertex        = -1;
                y_vertex        = -1;
                z_vertex        = -1;
                x_vertex_end    = -1;
                y_vertex_end    = -1;
                z_vertex_end    = -1;
                x_min           = -1;
                x_max           = -1;
                y_min           = -1;
                y_max           = -1;
                z_min           = -1;
                z_max           = -1;
                N_photons       =  0;
                x_min_cut       = -1;
                x_max_cut       = -1;
                y_min_cut       = -1;
                y_max_cut       = -1;
                z_min_cut       = -1;
                z_max_cut       = -1;
                N_photons_cut   =  0;
                proj_track_2D_cut = -1;
                px    = 0;
                py    = 0;
                pz    = 0;
                nhits_og = numhits;


                vector<vector<int>> background(x_pix,
                                               vector<int>(y_pix, 0));
                                               
                AddBckg(options, background);
                
                if (energy < stod(options["ion_pot"])){
                    energy = 0;
                    TH2I final_image(Form("pic_run%d_ev%d", run_count, entry), "",
                                     x_pix, -0.5, x_pix -0.5,
                                     y_pix, -0.5, y_pix -0.5);
                    
                    for(unsigned int xx =0; xx < background.size(); xx++) {
                        for(unsigned int yy =0; yy < background[0].size(); yy++) {
                            final_image.SetBinContent(xx+1, yy+1, background[xx][yy]);
                        }
                    }
                    
                    outtree->Fill();
                    outfile->cd();
                    final_image.Write();
                    
                    continue;
                }
                
                vector<double> x_hits_tr;
                vector<double> y_hits_tr;
                vector<double> z_hits_tr;
                

                if (options["SRIM"]=="True") {
                    // x_hits_tr = np.array(tree.x_hits) + opt.x_offset
                    // y_hits_tr = np.array(tree.y_hits) + opt.y_offset
                    // z_hits_tr = np.array(tree.z_hits) + opt.z_offset
                    vector<double> v1 = {1.,0.,0.};
                    vector<double> v2 = {stod(SRIM_events[entry][3])-stod(SRIM_events[entry][2]),
                                         stod(SRIM_events[entry][5])-stod(SRIM_events[entry][4]),
                                         stod(SRIM_events[entry][7])-stod(SRIM_events[entry][6]),
                                        };
                    
                    double        angle = angle_between(v1, v2);
                    vector<double> axis =  crossProduct(v1, v2);
                    
                    
                    double norm = sqrt(inner_product(axis.begin(), axis.end(), axis.begin(), 0.0));
                    vector<double> uaxis = {axis[0]/norm, axis[1]/norm, axis[2]/norm};
                    // DEBUG
                    //std::cout<<angle<<endl;
                    //std::cout<<"-"<<axis[0]<<","<<axis[1]<<","<<axis[2]<<endl;
                    
                    
                    for(int ihit=0; ihit < numhits; ihit++) {
                        vector<double> tmpvec = {(*x_hits)[ihit], (*y_hits)[ihit], (*z_hits)[ihit]};
                        vector<double> rotvec = rotateByAngleAndAxis(tmpvec, angle, uaxis);
                        
                        x_hits_tr.push_back(rotvec[0]+stod(SRIM_events[entry][2])+stod(options["x_offset"]));
                        y_hits_tr.push_back(rotvec[1]+stod(SRIM_events[entry][4])+stod(options["y_offset"]));
                        z_hits_tr.push_back(rotvec[2]+stod(SRIM_events[entry][6])+stod(options["z_offset"]));
                    }
                    
                } else {
                    transform(z_hits->begin(),
                              z_hits->end(),
                              back_inserter(x_hits_tr),
                              [&] (double a) {return a + stod(options["x_offset"]);});
                    transform(y_hits->begin(),
                              y_hits->end(),
                              back_inserter(y_hits_tr),
                              [&] (double a) {return a + stod(options["y_offset"]);});
                    transform(x_hits->begin(),
                              x_hits->end(),
                              back_inserter(z_hits_tr),
                              [&] (double a) {return a + stod(options["z_offset"]);});
                    
                    // FIXME: [Check which is the z axis orientation]
                    
                }
                
                // DEBUG
                //for(int ihit=0; ihit < numhits; ihit++) {
                //    cout<<x_hits_tr[ihit]<<",";
                //    cout<<y_hits_tr[ihit]<<",";
                //    cout<<z_hits_tr[ihit]<<"\n";
                //}
                
                vector<double> energy_hits = (*energyDep_hits);
                
                // add random Z to tracks
                if (stod(options["randZ_range"]) != 0) {
                    double rand = (gRandom->Uniform() - 0.5) * stod(options["randZ_range"]);
                    //DEBUG
                    //cout<<"rand = "<<rand<<endl;
                    transform(z_hits_tr.begin(), z_hits_tr.end(), z_hits_tr.begin(),
                              [&] (double a) {return a + rand;}
                              );
                    
                }
                
                //Compute length and extremes of the track before the cut
                proj_track_2D = 0;
                for(int ihit=0; ihit < numhits-1; ihit++){
                    proj_track_2D += sqrt((x_hits_tr[ihit+1]-x_hits_tr[ihit])*(x_hits_tr[ihit+1]-x_hits_tr[ihit])+
                                          (y_hits_tr[ihit+1]-y_hits_tr[ihit])*(y_hits_tr[ihit+1]-y_hits_tr[ihit])
                                          );
                }
                // DEBUG
                //cout<<"proj_track_2D = "<<Form("%.10f", proj_track_2D)<<endl;
                
                
                x_vertex = (x_hits_tr[0] + 0.5 * stod(options["x_dim"]) )*static_cast<double>(x_pix)/stod(options["x_dim"]); //in pixels
                y_vertex = (y_hits_tr[0] + 0.5 * stod(options["y_dim"]) )*static_cast<double>(y_pix)/stod(options["y_dim"]); //in pixels
                z_vertex = (z_hits_tr[0]+stod(options["z_extra"])); //distance from GEMs in mm
                // DEBUG
                //cout<<"x_vertex = "<<x_vertex<<" ### y_vertex = "<<y_vertex<<" ### z_vertex = "<<z_vertex<<endl;
                
                x_vertex_end = (x_hits_tr[numhits-1] + 0.5 * stod(options["x_dim"])) * static_cast<double>(x_pix) / stod(options["x_dim"]); //in pixels
                y_vertex_end = (y_hits_tr[numhits-1] + 0.5 * stod(options["y_dim"])) * static_cast<double>(y_pix) / stod(options["y_dim"]); //in pixels
                z_vertex_end = (z_hits_tr[numhits-1]+stod(options["z_extra"])); //distance from GEMs in mm
                //DEBUG
                //cout<<"x_vertex_end = "<<x_vertex_end<<" ### y_vertex_end = "<<y_vertex_end<<" ### z_vertex_end = "<<z_vertex_end<<endl;
                
                x_min = (*min_element(x_hits_tr.begin(), x_hits_tr.end()) + 0.5 * stod(options["x_dim"])) * static_cast<double>(x_pix) / stod(options["x_dim"]);
                x_max = (*max_element(x_hits_tr.begin(), x_hits_tr.end()) + 0.5 * stod(options["x_dim"])) * static_cast<double>(x_pix) / stod(options["x_dim"]);
                y_min = (*min_element(y_hits_tr.begin(), y_hits_tr.end()) + 0.5 * stod(options["y_dim"])) * static_cast<double>(y_pix) / stod(options["y_dim"]);
                y_max = (*max_element(y_hits_tr.begin(), y_hits_tr.end()) + 0.5 * stod(options["y_dim"])) * static_cast<double>(y_pix) / stod(options["y_dim"]);
                z_min = min((*max_element(z_hits_tr.begin(),
                                             z_hits_tr.end()) + stod(options["z_extra"])),
                            (*min_element(z_hits_tr.begin(),
                                             z_hits_tr.end()) + stod(options["z_extra"])));
                z_max = max((*max_element(z_hits_tr.begin(),
                                             z_hits_tr.end()) + stod(options["z_extra"])),
                            (*min_element(z_hits_tr.begin(),
                                             z_hits_tr.end()) + stod(options["z_extra"])));
                //DEBUG
                //cout<<" x_min = "<<x_min<<" x_max = "<<x_max<<" y_min = "<<y_min<<" y_max = "<<y_max<<" z_min = "<<z_min<<" z_max = "<<z_max<<endl;
                
                
                //CUT TRACKS due to exposure of camera
                double randcut = gRandom->Uniform(stod(options["exposure_time"])+readout_time);
                //randcut = 390.0;
                
                if (options["exposure_time_effect"] == "True") {
                    if (randcut<readout_time) {
                        
                        double y_cut_tmp = stod(options["y_dim"]) * (0.5 - randcut/readout_time)-3.;
                        
                        // Removing elements from x_hits_tr
                        x_hits_tr.erase(std::remove_if(x_hits_tr.begin(), x_hits_tr.end(), [&](const double& x) {
                            return y_hits_tr[&x-&*x_hits_tr.begin()] < y_cut_tmp;
                        }), x_hits_tr.end());
                        // Removing elements from z_hits_tr
                        z_hits_tr.erase(std::remove_if(z_hits_tr.begin(), z_hits_tr.end(), [&](const double& z) {
                            return y_hits_tr[&z-&*z_hits_tr.begin()] < y_cut_tmp;
                        }), z_hits_tr.end());
                        // Removing elements from energy_hits
                        energy_hits.erase(std::remove_if(energy_hits.begin(), energy_hits.end(), [&](const double& e) {
                            return y_hits_tr[&e-&*energy_hits.begin()] < y_cut_tmp;
                        }), energy_hits.end());
                        
                        // Removing elements from y_hits_tr [must be done after the previous ones]
                        y_hits_tr.erase(std::remove_if(y_hits_tr.begin(), y_hits_tr.end(),[&](const double& y) {
                            return y < y_cut_tmp;
                        }), y_hits_tr.end());
                        
                        row_cut = y_pix - (int)(randcut * static_cast<double>(y_pix) / readout_time);
                        
                        //DEBUG
                        //cout<<"y_cut_tmp = "<<y_cut_tmp<<endl;
                        //cout<<"row_cut = "<<row_cut<<endl;
                        //cout<<"sizes = ["<< x_hits_tr.size()<<","<<y_hits_tr.size()<<","
                        //    <<z_hits_tr.size()<<","<<energy_hits.size()<<"]"<<endl;
                        
                    } else if (randcut>stod(options["exposure_time"])) {
                        double y_cut_tmp = stod(options["y_dim"]) * (0.5 - (randcut - stod(options["exposure_time"])) / readout_time)+3.;
                        
                        // Removing elements from x_hits_tr
                        x_hits_tr.erase(std::remove_if(x_hits_tr.begin(), x_hits_tr.end(), [&](const double& x) {
                            return y_hits_tr[&x-&*x_hits_tr.begin()] > y_cut_tmp;
                        }), x_hits_tr.end());
                        // Removing elements from z_hits_tr
                        z_hits_tr.erase(std::remove_if(z_hits_tr.begin(), z_hits_tr.end(), [&](const double& z) {
                            return y_hits_tr[&z-&*z_hits_tr.begin()] > y_cut_tmp;
                        }), z_hits_tr.end());
                        // Removing elements from energy_hits
                        energy_hits.erase(std::remove_if(energy_hits.begin(), energy_hits.end(), [&](const double& e) {
                            return y_hits_tr[&e-&*energy_hits.begin()] > y_cut_tmp;
                        }), energy_hits.end());
                        
                        // Removing elements from y_hits_tr [must be done after the previous ones]
                        y_hits_tr.erase(std::remove_if(y_hits_tr.begin(), y_hits_tr.end(),[&](const double& y) {
                            return y > y_cut_tmp;
                        }), y_hits_tr.end());
                        
                        row_cut = y_pix - (int)((randcut-stod(options["exposure_time"])) * static_cast<double>(y_pix) / readout_time);
                        
                        //DEBUG
                        //cout<<"y_cut_tmp = "<<y_cut_tmp<<endl;
                        //cout<<"row_cut = "<<row_cut<<endl;
                        //cout<<"sizes = ["<< x_hits_tr.size()<<","<<y_hits_tr.size()<<","
                        //    <<z_hits_tr.size()<<","<<energy_hits.size()<<"]"<<endl;
                        
                    }
                    if(x_hits_tr.size()==0){
                        cut_energy = 0;
                        cout<<"The track was completely cut"<<endl;
                        TH2I final_image(Form("pic_run%d_ev%d", run_count, entry), "",
                                         x_pix, -0.5, x_pix -0.5,
                                         y_pix, -0.5, y_pix -0.5);
                        
                        for(unsigned int xx =0; xx < background.size(); xx++) {
                            for(unsigned int yy =0; yy < background[0].size(); yy++) {
                                final_image.SetBinContent(xx+1, yy+1, background[xx][yy]);
                            }
                        }
                        
                        outtree->Fill();
                        outfile->cd();
                        final_image.Write();
                        
                        continue;
                    }
                }
                
                vector<vector<double>> array2d_Nph(x_pix,
                                                   vector<double>(y_pix, 0.0));
                
                auto ta = std::chrono::steady_clock::now();
                // with saturation
                if(options["saturation"]=="True") {
                    cout<<"Starting compute_cmos_with_saturation with size = "<<x_hits_tr.size()<<"..."<<endl;
                    compute_cmos_with_saturation(x_hits_tr,
                                                 y_hits_tr,
                                                 z_hits_tr,
                                                 energy_hits,
                                                 options,
                                                 array2d_Nph,
                                                 energy,
                                                 NR_flag
                                                 );
                } else {// no saturation
                    compute_cmos_without_saturation(x_hits_tr,
                                                    y_hits_tr,
                                                    z_hits_tr,
                                                    energy_hits,
                                                    options,
                                                    array2d_Nph
                                                    );
                }
                
                // Integral of the track - if opt.exposure_effect, it's computed anyway after the cut on the original hits (to save time we digitize only the part that will be visible)
                N_photons = accumulate(array2d_Nph.cbegin(), array2d_Nph.cend(), 0, [](auto sum, const auto& row) {
                    return accumulate(row.cbegin(), row.cend(), sum);
                });
                // DEBUG
                cout<<"N_photons = "<<N_photons<<endl;
                
                if(options["Vignetting"]=="True") {
                    TrackVignetting(array2d_Nph,
                                    x_pix,
                                    y_pix,
                                    VignMap);
                }
                
                
                if (options["exposure_time_effect"]=="True") { //cut the track post-smearing
                    if (randcut<readout_time) {
                        for(unsigned int xx=0; xx < array2d_Nph.size(); xx++) {
                            for(int yy=0; yy < row_cut; yy++) {
                                array2d_Nph[xx][yy] = 0.0;
                            }
                        }
                    } else if(randcut> stod(options["exposure_time"]) ) {
                        for(unsigned int xx=0; xx < array2d_Nph.size(); xx++) {
                            for(int yy=row_cut; yy < (int)array2d_Nph[0].size(); yy++) {
                                array2d_Nph[xx][yy] = 0.0;
                            }
                        }
                    }
                    
                    //integral after camera exposure cut
                    N_photons_cut = accumulate(array2d_Nph.cbegin(),
                                               array2d_Nph.cend(), 0, [](auto sum, const auto& row) {
                        return accumulate(row.cbegin(), row.cend(), sum);
                    });
                }
                
                TH2I final_image(Form("pic_run%d_ev%d", run_count, entry), "",
                                 x_pix, -0.5, x_pix -0.5,
                                 y_pix, -0.5, y_pix -0.5);
                
                for(unsigned int xx =0; xx < array2d_Nph.size(); xx++) {
                    for(unsigned int yy =0; yy < array2d_Nph[0].size(); yy++) {
                        
                        int binc = background[xx][yy]+(int)array2d_Nph[xx][array2d_Nph[0].size()-1-yy];
                        final_image.SetBinContent(xx+1, yy+1, binc);
                    }
                }
                
                //Cut again the hits to save the effective length and energy which is visible in the final image,
                //and compute the number of photons post-cut
                if (options["exposure_time_effect"]=="True") {
                    if (randcut<readout_time) {
                        double y_cut_tmp = stod(options["y_dim"]) * (0.5 - randcut/readout_time);

                        // Removing elements from x_hits_tr
                        x_hits_tr.erase(std::remove_if(x_hits_tr.begin(), x_hits_tr.end(), [&](const double& x) {
                            return y_hits_tr[&x-&*x_hits_tr.begin()] < y_cut_tmp;
                        }), x_hits_tr.end());
                        // Removing elements from z_hits_tr
                        z_hits_tr.erase(std::remove_if(z_hits_tr.begin(), z_hits_tr.end(), [&](const double& z) {
                            return y_hits_tr[&z-&*z_hits_tr.begin()] < y_cut_tmp;
                        }), z_hits_tr.end());
                        // Removing elements from energy_hits
                        energy_hits.erase(std::remove_if(energy_hits.begin(), energy_hits.end(), [&](const double& e) {
                            return y_hits_tr[&e-&*energy_hits.begin()] < y_cut_tmp;
                        }), energy_hits.end());
                        
                        // Removing elements from y_hits_tr [must be done after the previous ones]
                        y_hits_tr.erase(std::remove_if(y_hits_tr.begin(), y_hits_tr.end(),[&](const double& y) {
                            return y < y_cut_tmp;
                        }), y_hits_tr.end());
                        
                    } else if (randcut> stod(options["exposure_time"]) ) {
                        double y_cut_tmp = stod(options["y_dim"]) * (0.5 - (randcut - stod(options["exposure_time"])) / readout_time);

                        // Removing elements from x_hits_tr
                        x_hits_tr.erase(std::remove_if(x_hits_tr.begin(), x_hits_tr.end(), [&](const double& x) {
                            return y_hits_tr[&x-&*x_hits_tr.begin()] > y_cut_tmp;
                        }), x_hits_tr.end());
                        // Removing elements from z_hits_tr
                        z_hits_tr.erase(std::remove_if(z_hits_tr.begin(), z_hits_tr.end(), [&](const double& z) {
                            return y_hits_tr[&z-&*z_hits_tr.begin()] > y_cut_tmp;
                        }), z_hits_tr.end());
                        // Removing elements from energy_hits
                        energy_hits.erase(std::remove_if(energy_hits.begin(), energy_hits.end(), [&](const double& e) {
                            return y_hits_tr[&e-&*energy_hits.begin()] > y_cut_tmp;
                        }), energy_hits.end());
                        
                        // Removing elements from y_hits_tr [must be done after the previous ones]
                        y_hits_tr.erase(std::remove_if(y_hits_tr.begin(), y_hits_tr.end(),[&](const double& y) {
                            return y > y_cut_tmp;
                        }), y_hits_tr.end());
                        
                    }
                    
                    cut_energy = accumulate(energy_hits.begin(), energy_hits.end(), 0.0);
                    
                    if(x_hits_tr.size()==0){
                        cut_energy = 0;
                        cout<<"The track was completely cut after smearing"<<endl;
                        
                        TH2I final_image_cut(Form("pic_run%d_ev%d", run_count, entry), "",
                                         x_pix, -0.5, x_pix -0.5,
                                         y_pix, -0.5, y_pix -0.5);
                        
                        for(unsigned int xx =0; xx < background.size(); xx++) {
                            for(unsigned int yy =0; yy < background[0].size(); yy++) {
                                final_image_cut.SetBinContent(xx+1, yy+1, background[xx][yy]);
                            }
                        }
                        
                        outtree->Fill();
                        outfile->cd();
                        final_image_cut.Write();
                        
                        continue;
                    }
                    
                }
                
                
                // Compute variables to be saved in the tree
                proj_track_2D_cut = 0;
                for(int ihit=0; ihit < numhits-1; ihit++){
                    proj_track_2D_cut += sqrt((x_hits_tr[ihit+1]-x_hits_tr[ihit])*(x_hits_tr[ihit+1]-x_hits_tr[ihit])+
                                              (y_hits_tr[ihit+1]-y_hits_tr[ihit])*(y_hits_tr[ihit+1]-y_hits_tr[ihit])
                                             );
                }
                // DEBUG
                //cout<<"proj_track_2D_cut = "<<Form("%.10f", proj_track_2D_cut)<<endl;
                
                
                x_min_cut = (*min_element(x_hits_tr.begin(), x_hits_tr.end()) + 0.5 * stod(options["x_dim"])) * static_cast<double>(x_pix) / stod(options["x_dim"]);
                x_max_cut = (*max_element(x_hits_tr.begin(), x_hits_tr.end()) + 0.5 * stod(options["x_dim"])) * static_cast<double>(x_pix) / stod(options["x_dim"]);
                y_min_cut = (*min_element(y_hits_tr.begin(), y_hits_tr.end()) + 0.5 * stod(options["y_dim"])) * static_cast<double>(y_pix) / stod(options["y_dim"]);
                y_max_cut = (*max_element(y_hits_tr.begin(), y_hits_tr.end()) + 0.5 * stod(options["y_dim"])) * static_cast<double>(y_pix) / stod(options["y_dim"]);
                z_min_cut = min((*max_element(z_hits_tr.begin(),
                                                 z_hits_tr.end()) + stod(options["z_extra"])),
                                (*min_element(z_hits_tr.begin(),
                                                 z_hits_tr.end()) + stod(options["z_extra"])));
                z_max_cut = max((*max_element(z_hits_tr.begin(),
                                                 z_hits_tr.end()) + stod(options["z_extra"])),
                                (*min_element(z_hits_tr.begin(),
                                                 z_hits_tr.end()) + stod(options["z_extra"])));
                //DEBUG
                //cout<<" x_min_cut = "<<x_min_cut<<" x_max_cut = "<<x_max_cut<<" y_min_cut = "<<y_minv<<" y_max_cut = "<<y_max_cut<<" z_min_cut = "<<z_min_cut<<" z_max_cut = "<<z_max_cut<<endl;
                
                
                // if there are at least 2 hits compute theta and phi
                if(x_hits_tr.size() > 1) {
                    phi   = atan2( y_hits_tr[1] - y_hits_tr[0],
                                   x_hits_tr[1] - x_hits_tr[0]
                                  );
                    theta = acos((z_hits_tr[1] - z_hits_tr[0]) /
                                 sqrt(
                                     (x_hits_tr[1] - x_hits_tr[0])*(x_hits_tr[1] - x_hits_tr[0])+
                                     (y_hits_tr[1] - y_hits_tr[0])*(y_hits_tr[1] - y_hits_tr[0])+
                                     (z_hits_tr[1] - z_hits_tr[0])*(z_hits_tr[1] - z_hits_tr[0]))
                                 );
                } else {
                    phi   = -999;
                    theta = -999;
                }
                
                if(options["SRIM"]=="False") {
                    track_length_3D = accumulate(tracklen_hits->begin(), tracklen_hits->end(), 0.0);
                    px              = (*px_particle)[0];
                    py              = (*py_particle)[0];
                    pz              = (*pz_particle)[0];
                }
                
                auto tb = std::chrono::steady_clock::now();
                std::chrono::duration<double> durtmp=tb-ta;
                cout << "Time taken in seconds to compute_cmos_with_saturation is: " << durtmp.count() << endl;
                
                
                outtree->Fill();
                outfile->cd();
                
                // DEBUG
                final_image.Write();
                
            }
            //outfile->cd("event_info");
            outtree->Write();
            
            cout<<Form("COMPLETED RUN %d",run_count)<<endl;
            
            f->Close();
            outfile->Close();
            
            run_count ++;
        }
        
    }
	
    auto t1 = std::chrono::steady_clock::now();
    std::chrono::duration<double> dur=t1-t0;
    cout << "Time taken in seconds is: " << dur.count() << endl;
    return 0;
    
}

// ====== FUNCTIONS DEFINITION ======
vector<string> split_str(const string& str, char delimiter)
{
    vector<string> tokens;
    istringstream stream(str);
    string token;

    while(getline(stream,token,delimiter)) tokens.push_back(token);

    return tokens;
}


void ReadConfig(string name, map<string,string>& options)
{
    ifstream config(name.c_str());
	
    string line;
    while(getline(config,line))
    {
        line.erase(remove_if(line.begin(),line.end(),[] (char c){return isspace(c);}),line.end());
        line.erase(remove_if(line.begin(),line.end(),[] (char c){return c=='\'';}),line.end());
        if(line[0] == '#' || line.empty() || line[0] == '{' || line[0] == '}') continue;
		
        auto delim1= line.find(":");
        auto delim2= line.find(",");
        if(delim2==string::npos) delim2=line.size();
        auto index= line.substr(0,delim1);
        auto val= line.substr(delim1+1,min(delim2,line.size())-delim1-1 );
        options[index]=val;
    }
	
}

void ReadG4Isotopes(string name, map<string,string>& dict_isotopes) {
    ifstream config(name.c_str());
    
    string line;
    while(getline(config, line))
    {
        line.erase(remove_if(line.begin(),line.end(),[] (char c){return isspace(c);}),line.end());
        line.erase(remove_if(line.begin(),line.end(),[] (char c){return c=='\'';}),line.end());
        if(line[0] == '#' || line.empty() || line[0] == '{' || line[0] == '}') continue;
        
        stringstream to_split(line.c_str());
        string element;
        
        int counter = 0;
        string index;
        string val;
        while(getline(to_split, element, ',')) {
            if(counter == 0) index = element;
            else if (counter == 1) val = element;
            else if (counter == 2) val += Form("%03d", stoi(element));
            counter++;
        }
        dict_isotopes[index]=val;
        
        
        // DEBUG
        // cout<<"---"<<index<<": "<<val<<endl;
    }
    
    return;
}


void ReadIonlist(string name, vector<vector<string>>& ionlist) {
    
    cout<<"Opening and parsing "<<name<<" ..."<<endl;
    ifstream config(name.c_str());
    
    string line;
    while(getline(config, line))
    {
        line.erase(remove_if(line.begin(),line.end(),[] (char c){return isspace(c);}),line.end());
        if(line.find("ionlist")!=string::npos) continue;
        stringstream to_split(line.c_str());
        
        string element;
        while(getline(to_split, element, '[')) {
            if(!element.empty()){
                element.erase(remove_if(element.begin(),element.end(),[=] (char c){return c==']';}), element.end());
                stringstream content(element.c_str());

                string var;
                vector<string> row;
                while(getline(content, var, ',')) {
                    if(!var.empty()){
                        row.push_back(var);
                    }
                }
                ionlist.push_back(row);
            }
        }
        break;
    }
    return;
}

vector<double> NelGEM1(const vector<double>& N_ioniz_el) {
    
    vector<double> n_tot_el(N_ioniz_el.size(), 0);
    
    if (N_ioniz_el.size() == 1) { // in case there is only one hit (very low energy)
        //for(int j = 0; j<(int)N_ioniz_el[0]; j++) {
            //double nsec = gRandom->Exp(GEM1_gain) * extraction_eff_GEM1;
            //n_tot_el[0] += nsec;
        n_tot_el[0] = N_ioniz_el[0];
        //}
    } else {
        for(unsigned int i = 0; i<N_ioniz_el.size(); i++) {
            for(int j = 0; j<(int)round(N_ioniz_el[i]); j++) {
                double nsec = gRandom->Exp(GEM1_gain) * extraction_eff_GEM1;
                n_tot_el[i] += nsec;
            }
        }
    }
    //DEBUG
    //for(unsigned int i=0; i<n_tot_el.size(); i++) {
    //    cout<<n_tot_el[i]<<"\n";
    //}
    
    return n_tot_el;
}


vector<double> NelGEM2(const vector<double>& energyDep, const vector<double>& z_hit, map<string,string>& options) {
    vector<double> n_ioniz_el_ini;
    double opt_pot=stod(options["ion_pot"]);
    transform(energyDep.begin(),energyDep.end(),back_inserter(n_ioniz_el_ini), [&] (double a) { return a/opt_pot;});
    
    vector<double> drift_l;
    int opt_gem=stod(options["z_extra"]);
    transform(z_hit.begin(),z_hit.end(),back_inserter(drift_l), [&] (double a) { return a+opt_gem;});
    
    vector<double> n_ioniz_el_mean(n_ioniz_el_ini.size(), 0.0);
    
    double optabsorption_l=stod(options["absorption_l"]);
    for(unsigned int i=0;i<n_ioniz_el_mean.size();i++) n_ioniz_el_mean[i]=abs(n_ioniz_el_ini[i]*exp(-drift_l[i]/optabsorption_l));
    
    vector<double> n_ioniz_el(n_ioniz_el_ini.size(), 0);
    transform(n_ioniz_el_mean.begin(), n_ioniz_el_mean.end(), n_ioniz_el.begin(), [&] (double a) {
        return gRandom->Poisson(a);
    });
    
    // total number of secondary electrons considering the gain in the 2nd GEM foil
    vector<double> n_tot_el = NelGEM1(n_ioniz_el);
    transform(n_tot_el.begin(), n_tot_el.end(), n_tot_el.begin(), [&] (double a) {
        return round(a * GEM2_gain * extraction_eff_GEM2);
    });
    
    return n_tot_el;
    
}

void SaveValues(map<string,string>& options, shared_ptr<TFile>& outfile)
{
    outfile->cd();
    outfile->mkdir("param_dir");
    outfile->cd("param_dir");
    for(auto const& [key, val] : options)
    {
        // DEBUG
        // cout<<key<<": "<<val<<endl;
        
        if(key!="tag"       && key !="Vig_Map" &&
           key!="bckg_path" && key !="ped_cloud_dir" &&
           key!="noiserun"  && key !="bckg_name" &&
           key!="NR_list"   && key !="Camera_type"
           )
        {
            TH1F h(string(key).c_str(),"",1,0,1);
            
            double value;
            if(val == "True")       value = 1.;
            else if(val == "False") value = 0.;
            else                   value  = stod(val);
            
            h.SetBinContent(1,value);
            h.Write();
        }
    }
    
    if(options["bckg"]=="True") {
        std::vector<int> pedruns;
        
        stringstream ssruns(options["noiserun"]);
        string run;
        vector<string> seglist;
        while(getline(ssruns, run, ';')) {
            int runi = stoi(run);
            pedruns.push_back(runi);
        }
        
        int npeds = (int)pedruns.size();
        TH1F h("pedestal_runs","",npeds,0,npeds);
        for (int i = 0; i < npeds; i++) {
            h.SetBinContent(i+1, pedruns[i]);
        }
        h.Write();
        
    } else {
        TH1F h("pedestal_runs","",1,0,1);
        h.SetBinContent(1, -1);
        h.Write();
    }
    
    
    outfile->cd();
    return;
}

void AddBckg(map<string,string>& options, vector<vector<int>>& background) {
    
    if(options["bckg"]=="True") {
        
        string tmpfolder = options["bckg_path"];
        string tmpname   = options["bckg_name"];
        
        if(! filesystem::exists(tmpfolder)){
            //DEBUG
            cout<<"Creating tmpfolder..."<<
            system(("mkdir " + tmpfolder).c_str() );
        }
        
        if(! filesystem::exists(tmpfolder+tmpname)) {
            
            vector<int> pedruns;
            
            stringstream ssruns(options["noiserun"]);
            string run;
            vector<string> seglist;
            while(getline(ssruns, run, ';')) {
                int runi = stoi(run);
                pedruns.push_back(runi);
            }
            
            cygnolib::joinMidasPedestals(pedruns, options["ped_cloud_dir"], tmpfolder, tmpname);
            
        }
        
        
        //throw runtime_error("DEBUG");
        
        auto fin = unique_ptr<TFile> {TFile::Open((tmpfolder+tmpname).c_str())};
        
        // Computing number of images in root file
        int flength = 0;
        for (auto&& keyAsObj : *(fin->GetListOfKeys())){
            if(false) cout<<keyAsObj<<endl; //just to avoid a warning in the compilation
            flength++;
        }
        
        int pic_index = 0;
        if(stoi(options["random_ped"])==-1) pic_index = gRandom->Integer(flength);
        else pic_index =stoi(options["random_ped"]);
        
        cout<<"Using pic # "<<pic_index<<" out of "<<flength<<" as a pedestal..."<<endl;
        TH2I* pic = fin->Get<TH2I>(Form("pic_%d", pic_index));

        int x_ped=pic->GetNbinsX();
        int y_ped=pic->GetNbinsY();
        //Check pedestal and simulation have same camera settings
        if(x_ped!=y_ped)
        {
            if(x_pix==y_pix)
            {
                cerr<<"You are using a Quest pedestal map with a Fusion simulation! Digitization FAILED!\n";
                exit(0);
            }
        }
        else
        {
            if(x_pix!=y_pix)
            {
                cerr<<"You are using a Fusion pedestal map with a Quest simulation! Digitization FAILED!\n";
                exit(0);
            }

        }


        for(int i = 0; i<pic->GetNbinsX(); i++) {
            for (int j =0; j<pic->GetNbinsY(); j++) {
                background[i][j] = pic->GetBinContent(i+1,j+1);
            }
        }
        //throw runtime_error("DEBUG");
    }
    return;
}


bool is_NR(vector<int> pdgID_hits, int pdg) {
    int ret = -999;
    for(unsigned int i = 0; i<pdgID_hits.size(); i++) {
        if(pdgID_hits[i] > pdg) {
            ret = pdgID_hits[i];
            break;
        }
    }
    return ret;
}

// Returns the angle in radians between vectors 'v1' and 'v2'
// >>> angle_between((1, 0, 0), (0, 1, 0))
// 1.5707963267948966
// >>> angle_between((1, 0, 0), (1, 0, 0))
// 0.0
// >>> angle_between((1, 0, 0), (-1, 0, 0))
// 3.141592653589793

double angle_between(vector<double>& v1, vector<double>& v2) {
    if (v2.size() != 3 || v1.size() != 3) {
        throw std::invalid_argument("angle_between: Both input vectors must have exactly 3 elements.");
    }
    
    double dot    = inner_product(v1.begin(), v1.end(), v2.begin(), 0.0);
    double lenSq1 = inner_product(v1.begin(), v1.end(), v1.begin(), 0.0);
    double lenSq2 = inner_product(v2.begin(), v2.end(), v2.begin(), 0.0);
    
    //DEBUG
    //cout<<"~"<<dot<<","<<lenSq1<<","<<lenSq2<<endl;
    
    double angle = acos(dot/sqrt(lenSq1 * lenSq2));
    return angle;
}

vector<double> crossProduct(vector<double>& a, vector<double>& b) {
    if (a.size() != 3 || b.size() != 3) {
        throw std::invalid_argument("crossProduct: Both input vectors must have exactly 3 elements.");
    }
    vector<double> result(3);
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
    return result;
}


vector<double> rotateByAngleAndAxis(vector<double>& vec, double angle, vector<double>& axis) {
    if (vec.size() != 3 || axis.size() != 3) {
        throw std::invalid_argument("rotateByAngleAndAxis: Both input vectors must have exactly 3 elements.");
    }
    
    // v_rot = (costheta)v + (sintheta)(axis x v) + (1-cos(theta)) (axis dot v) axis
    vector<double> result(3);
    
    vector<double> axisXvec = crossProduct(axis, vec);
    double axisDOTvec       = inner_product(axis.begin(), axis.end(), vec.begin(), 0.0);
    
    result[0] = cos(angle) * vec[0] + sin(angle) * axisXvec[0] + (1.-cos(angle))*axisDOTvec*axis[0];
    result[1] = cos(angle) * vec[1] + sin(angle) * axisXvec[1] + (1.-cos(angle))*axisDOTvec*axis[1];
    result[2] = cos(angle) * vec[2] + sin(angle) * axisXvec[2] + (1.-cos(angle))*axisDOTvec*axis[2];
    
    return result;
}

void compute_cmos_with_saturation(vector<double>& x_hits_tr,
                                  vector<double>& y_hits_tr,
                                  vector<double>& z_hits_tr,
                                  vector<double>& energy_hits,
                                  map<string,string>& options,
                                  vector<vector<double>>& array2d_Nph,
                                  Float_t energy,
                                  bool NR_flag
                                  ) {
    
    // vectorized smearing
    vector<float> S3D_x;
    vector<float> S3D_y;
    vector<float> S3D_z;
    
    // if there are no electrons on GEM3, just use empty image
    if (x_hits_tr.size() == 0) return;
    // if there are electrons on GEM3, apply saturation effect
    else {
        double OFF = 15;
        double OFFz = 10.;

        double xmin = (*min_element(x_hits_tr.begin(), x_hits_tr.end()))-OFF;
        double xmax = (*max_element(x_hits_tr.begin(), x_hits_tr.end()))+OFF;
        double ymin = (*min_element(y_hits_tr.begin(), y_hits_tr.end()))-OFF;
        double ymax = (*max_element(y_hits_tr.begin(), y_hits_tr.end()))+OFF;
        double zmin = (*min_element(z_hits_tr.begin(), z_hits_tr.end()))-OFFz;
        double zmax = (*max_element(z_hits_tr.begin(), z_hits_tr.end()))+OFFz;
        
        double deltaX = abs(xmax-xmin);
        double deltaY = abs(ymax-ymin);
        
        // FIXME: create a function for the saturation loop
        // FIXME: find best value of maxvolume. 1e8 might not me the best one
        long int max_3Dhisto_volume=(long int)5e+7;  // (volume in number of voxels) that's around 0.5*1.6 GB of RAM

        int xy_vox_scale = stoi(options["xy_vox_scale"]);
        
        double xbin_dim = stod(options["x_dim"]) / x_pix / xy_vox_scale;
        double ybin_dim = stod(options["y_dim"]) / y_pix / xy_vox_scale;
        double zbin_dim = stod(options["z_vox_dim"]);
        
        
        long int x_n_bin = round_up_to_even((xmax-xmin)/xbin_dim);
        long int y_n_bin = round_up_to_even((ymax-ymin)/ybin_dim);
        long int z_n_bin_MAX = round_up_to_even((zmax-zmin)/zbin_dim);
        
        // If voxel regions <= 2 -> use the standard algorithm, otherwise use the map algorithm
        bool map_algorithm = true;
        if(energy/(x_n_bin * y_n_bin * z_n_bin_MAX)>1e-6) map_algorithm = false;

        long int z_n_bin;
        if (map_algorithm) z_n_bin = z_n_bin_MAX;
        
        // DEBUG
        //cout<<"size = "<<S3D_z.size()<<endl;
        //std::chrono::duration<double> dur=endsmear-startsmear;
        //std::cout << "Time smear " << dur.count() << " seconds" <<std::endl;
        
        double optA                 = stod(options["A"]);
        double optbeta              = stod(options["beta"]);
        double optphotons_per_el    = stod(options["photons_per_el"]);

        //Timing and debug variables
        long int size_tot=0;
        std::chrono::duration<double> dur_smear;
        std::chrono::duration<double> dur_criti;
        std::chrono::duration<double> dur_ampli;
        
        vector<vector<int>> hout(x_n_bin, vector<int>(y_n_bin, 0.0));
        
        if(map_algorithm) {
            map<long int, int> hcmap;
            //long int L = x_n_bin-1;
            long int M = y_n_bin;
            long int N = z_n_bin;
            
            if((NR_flag==true && energy>100)||(NR_flag==false && energy>500)) {
                
                int WID = stoi(options["WID"]);
                int nparts = 1 + x_hits_tr.size()/WID;
                for(int part = 0; part < nparts; part++) {
                    cout<<"   part "<<part<<"/"<<nparts<<"..."<<endl;
                    int hit_tr_idx = part*WID;
                    int hit_tr_idx_up = min((part+1)*WID,(int)x_hits_tr.size());
                    S3D_x.clear();
                    S3D_y.clear();
                    S3D_z.clear();
                    
                    vector<double> x_hits_tr_i(&x_hits_tr[hit_tr_idx], &x_hits_tr[hit_tr_idx_up]);
                    vector<double> y_hits_tr_i(&y_hits_tr[hit_tr_idx], &y_hits_tr[hit_tr_idx_up]);
                    vector<double> z_hits_tr_i(&z_hits_tr[hit_tr_idx], &z_hits_tr[hit_tr_idx_up]);
                    vector<double> energy_hits_i(&energy_hits[hit_tr_idx], &energy_hits[hit_tr_idx_up]);
                    
                    auto startsmear = std::chrono::steady_clock::now();
                    cloud_smearing3D(x_hits_tr_i, y_hits_tr_i, z_hits_tr_i, energy_hits_i, options, S3D_x, S3D_y, S3D_z);
                    auto endsmear = std::chrono::steady_clock::now();
                    dur_smear=dur_smear+endsmear-startsmear;
                    size_tot+=S3D_z.size();
                    
                    vector<size_t> indices(S3D_z.size());
                    // Fill indices with 0, 1, 2, ..., numbers.size() - 1
                    iota(indices.begin(), indices.end(), 0);
                    
                    auto startcriti = std::chrono::steady_clock::now();
                    // THIS IS THE COMPUTATIONALLY EXPENSIVE PART
                    //Parallel version
                    //Types of Mutex: spin -> unfair, unscalable (better for locking things that require little time)
                    //queue ->> fair, scalable
                    //tbb::queuing_mutex myMutex;
                    tbb::spin_mutex myMutex;
                    // Run the default parallelism
                    tbb::task_arena arena(num_threads);
                    arena.execute([&]{
                        parallel_for(tbb::blocked_range<size_t>(0, indices.size(),indices.size()/num_threads),
                            [&] (auto &range) {
                            map<long int, int> paralmap;
                            //cout << "Thread ID: " << this_thread::get_id() << endl;
                            for(auto ihit=range.begin();ihit<range.end();++ihit)
                            {
                                long int xx = floor((S3D_x[ihit]- xmin)/ xbin_dim);
                                long int yy = floor((S3D_y[ihit]- ymin)/ ybin_dim);
                                long int zz = floor((S3D_z[ihit]- zmin)/ zbin_dim);
                                long int map_index = xx * (M * N) + yy * N + zz;
                                
                                if (paralmap.find(map_index) == paralmap.end()) paralmap[map_index] = 1;
                                else paralmap[map_index] += 1;
                            }
                            //cout<<"Mutex\n";
                            //cout << "Thread ID: " << this_thread::get_id() << endl;
                            //tbb::queuing_mutex::scoped_lock myLock(myMutex);
                            tbb::spin_mutex::scoped_lock myLock(myMutex);	//maybe better to use oneapi::tbb::spin_mutex to use spi_mutex since the critical operation is small
                            //merge of paralmap in hcmap
                            hcmap.merge(paralmap);
                            for(auto it=paralmap.begin();it!=paralmap.end();++it) hcmap[it->first] = hcmap[it->first] + paralmap[it->first];               //Here auto would be std:map<long int,int>::iterator
                        });
                    });
                    
                    //End parallel version

                    //Sequencial version
                    /*for_each(indices.begin(), indices.end(), [&](int ihit) {
                        long int xx = floor((S3D_x[ihit]- xmin)/ xbin_dim);
                        long int yy = floor((S3D_y[ihit]- ymin)/ ybin_dim);
                        long int zz = floor((S3D_z[ihit]- zmin)/ zbin_dim);
                        long int map_index = xx * (M * N) + yy * N + zz;
                        
                        if (hcmap.find(map_index) == hcmap.end()) hcmap[map_index] = 1;
                        else hcmap[map_index] += 1;
                        
                    });*/
                    //End sequencial version
                    auto endcriti = std::chrono::steady_clock::now();
                    dur_criti=dur_criti+endcriti-startcriti;
                }
                auto startampli = std::chrono::steady_clock::now();
                for(auto const& [key, val] : hcmap) {
                    int zz = key % N ;
                    int yy = ((key - zz) / N) % M;
                    int xx = (key - yy * N - zz) / N / M;
                    // optbeta is multiplied by factors to normalize on volume chosen
                    hout[xx][yy]+=Nph_saturation(val, optA, optbeta*xy_vox_scale*xy_vox_scale*0.1/zbin_dim);
                }
                auto endampli = std::chrono::steady_clock::now();
                dur_ampli=dur_ampli+endampli-startampli;

            } else {

                auto startsmear = std::chrono::steady_clock::now();
                cloud_smearing3D(x_hits_tr, y_hits_tr, z_hits_tr, energy_hits, options, S3D_x, S3D_y, S3D_z);
                auto endsmear = std::chrono::steady_clock::now();
                dur_smear=dur_smear+endsmear-startsmear;
                size_tot+=S3D_z.size();

                vector<size_t> indices(S3D_z.size());
                // Fill indices with 0, 1, 2, ..., numbers.size() - 1
                iota(indices.begin(), indices.end(), 0);
                
                auto startcriti = std::chrono::steady_clock::now();
                //Parallel version
                //Types of Mutex: spin -> unfair, unscalable (better for locking things that require little time)
                //queue ->> fair, scalable
                //tbb::queuing_mutex myMutex;
                tbb::spin_mutex myMutex;
                // Run the default parallelism
                tbb::task_arena arena(num_threads);
                arena.execute([&]{
                    parallel_for(tbb::blocked_range<size_t>(0, indices.size(),indices.size()/num_threads),
                        [&] (auto &range) {
                        map<long int, int> paralmap;
                        //cout << "Thread ID: " << this_thread::get_id() << endl;
                        for(auto ihit=range.begin();ihit<range.end();++ihit)
                        {
                            long int xx = floor((S3D_x[ihit]- xmin)/ xbin_dim);
                            long int yy = floor((S3D_y[ihit]- ymin)/ ybin_dim);
                            long int zz = floor((S3D_z[ihit]- zmin)/ zbin_dim);
                            long int map_index = xx * (M * N) + yy * N + zz;
                            
                            if (paralmap.find(map_index) == paralmap.end()) paralmap[map_index] = 1;
                            else paralmap[map_index] += 1;
                        }
                        //cout<<"Mutex\n";
                        //cout << "Thread ID: " << this_thread::get_id() << endl;
                        //tbb::queuing_mutex::scoped_lock myLock(myMutex);
                        tbb::spin_mutex::scoped_lock myLock(myMutex);	//maybe better to use oneapi::tbb::spin_mutex to use spi_mutex since the critical operation is small
                        //merge of paralmap in hcmap
                        hcmap.merge(paralmap);
                        for(auto it=paralmap.begin();it!=paralmap.end();++it) hcmap[it->first] = hcmap[it->first] + paralmap[it->first];               //Here auto would be std:map<long int,int>::iterator
                    });
                });
                
                //End parallel version

                //Sequencial version
                /*for_each(indices.begin(), indices.end(), [&](int ihit) {
                    long int xx = floor((S3D_x[ihit]- xmin)/ xbin_dim);
                    long int yy = floor((S3D_y[ihit]- ymin)/ ybin_dim);
                    long int zz = floor((S3D_z[ihit]- zmin)/ zbin_dim);
                    long int map_index = xx * (M * N) + yy * N + zz;
                    
                    if (hcmap.find(map_index) == hcmap.end()) hcmap[map_index] = 1;
                    else hcmap[map_index] += 1;
                    
                });*/
                //End sequencial version
                auto endcriti = std::chrono::steady_clock::now();
                dur_criti=dur_criti+endcriti-startcriti;

                auto startampli = std::chrono::steady_clock::now();
                for(auto const& [key, val] : hcmap) {
                    int zz = key % N ;
                    int yy = ((key - zz) / N) % M;
                    int xx = (key - yy * N - zz) / N / M;
                    // optbeta is multiplied by factors to normalize on volume chosen
                    hout[xx][yy]+=Nph_saturation(val, optA, optbeta*xy_vox_scale*xy_vox_scale*0.1/zbin_dim);
                }
                auto endampli = std::chrono::steady_clock::now();
                dur_ampli=dur_ampli+endampli-startampli;
                
            }

        } else {
            double deltaZ=max(2*stod(options["z_vox_dim"]),
                              stod(options["z_vox_dim"]) * max_3Dhisto_volume /
                                (deltaX / xbin_dim) /
                                (deltaY / ybin_dim)
                              );
            
            vector<double> split_vals = arange(zmin, zmax, deltaZ);
            if(split_vals[split_vals.size()-1] < zmax) split_vals.push_back(zmax+deltaZ/10.);
        
        
        
            for(unsigned int i=0; i < split_vals.size()-1; i++) {
                
                z_n_bin  = max(2.0, round_up_to_even((split_vals[i+1]-split_vals[i])/zbin_dim));
                
                vector<vector<vector<double>>> hc(x_n_bin+1,
                                                  vector<vector<double>>(y_n_bin+1,
                                                                         vector<double>(z_n_bin+1, 0.0)));
                
                if((NR_flag==true && energy>100)||(NR_flag==false && energy>500)) {
                    

                    int WID = stoi(options["WID"]);
                    int nparts = 1 + x_hits_tr.size()/WID;
                    
                    //cout<<"DEBUG "<<hc.size()<<","<<hc[0].size()<<","<<hc[0][0].size()<<endl<<flush;
                    cout<<"Amplifying voxel region z=["<<split_vals[i]<<","<<split_vals[i+1]<<"] "<<i<<"/"<<split_vals.size()-1-1<<endl;
                    
                    for(int part = 0; part < nparts; part++) {
                        cout<<"   part "<<part<<"/"<<nparts<<"..."<<endl;
                        int hit_tr_idx = part*WID;
                        int hit_tr_idx_up = min((part+1)*WID,(int)x_hits_tr.size());
                        S3D_x.clear();
                        S3D_y.clear();
                        S3D_z.clear();
                    
                        vector<double> x_hits_tr_i(&x_hits_tr[hit_tr_idx], &x_hits_tr[hit_tr_idx_up]);
                        vector<double> y_hits_tr_i(&y_hits_tr[hit_tr_idx], &y_hits_tr[hit_tr_idx_up]);
                        vector<double> z_hits_tr_i(&z_hits_tr[hit_tr_idx], &z_hits_tr[hit_tr_idx_up]);
                        vector<double> energy_hits_i(&energy_hits[hit_tr_idx], &energy_hits[hit_tr_idx_up]);
                        
                        //cout<<"Smearing..."<<endl<<flush;
                        auto startsmear = std::chrono::steady_clock::now();
                        cloud_smearing3D(x_hits_tr_i, y_hits_tr_i, z_hits_tr_i, energy_hits_i, options, S3D_x, S3D_y, S3D_z);
                        auto endsmear = std::chrono::steady_clock::now();
                        dur_smear=dur_smear+endsmear-startsmear;
                        size_tot+=S3D_z.size();

                        //cout<<"----- x "<<S3D_x.size()<<endl<<flush;
                        //cout<<"----- y "<<S3D_y.size()<<endl<<flush;
                        //cout<<"----- z "<<S3D_z.size()<<endl<<flush;
                        
                        //cout<<"Getting the vector to store all indices"<<endl<<flush;
                        // Vector to store all indices
                        vector<size_t> allindices(S3D_z.size());
                        // Fill indices with 0, 1, 2, ..., numbers.size() - 1
                        iota(allindices.begin(), allindices.end(), 0);
                        
                        auto startcriti = std::chrono::steady_clock::now();
                        //cout<<"Getting the indices where split_vals[i] <= S3D_z < split_vals[i+1]"<<endl<<flush;
                        // Getting the indices where split_vals[i] <= S3D_z < split_vals[i+1]
                        vector<size_t> indices;
                        copy_if(allindices.begin(), allindices.end(), back_inserter(indices), [&](size_t n) {
                            return ( S3D_z[n] >= split_vals[i] && S3D_z[n] < split_vals[i+1]);
                        });
                        
                        if(indices.size()==0) continue;
                        
                        // THIS IS THE COMPUTATIONALLY EXPENSIVE PART
                        for_each(indices.begin(), indices.end(), [&](int ihit) {
                            int xx = floor((S3D_x[ihit]- xmin)/ xbin_dim);
                            int yy = floor((S3D_y[ihit]- ymin)/ ybin_dim);
                            int zz = floor((S3D_z[ihit]-split_vals[i])/ zbin_dim);
                            hc[xx][yy][zz] += 1.;
                        });
                        auto endcriti = std::chrono::steady_clock::now();
                        dur_criti=dur_criti+endcriti-startcriti;

                    }

                    long int LL = (x_n_bin)*(y_n_bin)*(z_n_bin);
                    long int not_empty=0;
                    auto startampli = std::chrono::steady_clock::now();
                    
                    // Applying GEM3 amplification
                    for(int xx = 0; xx<x_n_bin-1; xx++){
                        for(int yy=0; yy<y_n_bin-1; yy++) {
                            for(int zz=0; zz<z_n_bin-1; zz++){
                                if(hc[xx][yy][zz] != 0.) {
                                    not_empty++;
                                    // optbeta is multiplied by factors to normalize on volume chosen
                                    hout[xx][yy]+=Nph_saturation(hc[xx][yy][zz], optA, optbeta*xy_vox_scale*xy_vox_scale*0.1/zbin_dim);
                                }
                            }
                        }
                    }
                    auto endampli = std::chrono::steady_clock::now();
                    dur_ampli=dur_ampli+endampli-startampli;
                    cout<<"Sparse-ness of voxel region: "<< not_empty * 100./LL<<" %"<<endl;


                } else {
                    auto startsmear = std::chrono::steady_clock::now();
                    cloud_smearing3D(x_hits_tr, y_hits_tr, z_hits_tr, energy_hits, options, S3D_x, S3D_y, S3D_z);
                    auto endsmear = std::chrono::steady_clock::now();
                    dur_smear=dur_smear+endsmear-startsmear;
                    size_tot+=S3D_z.size();

                    // Vector to store all indices
                    vector<size_t> allindices(S3D_z.size());
                    // Fill indices with 0, 1, 2, ..., numbers.size() - 1
                    iota(allindices.begin(), allindices.end(), 0);
                    
                    auto startcriti = std::chrono::steady_clock::now(); 
                    // Getting the indices where split_vals[i] <= S3D_z < split_vals[i+1]
                    vector<size_t> indices;
                    copy_if(allindices.begin(), allindices.end(), back_inserter(indices), [&](size_t n) {
                        return ( S3D_z[n] >= split_vals[i] && S3D_z[n] < split_vals[i+1]);
                    });
                    
                    if(indices.size()==0) continue;
                    
                    //cout<<"DEBUG "<<hc.size()<<","<<hc[0].size()<<","<<hc[0][0].size()<<endl<<flush;
                    cout<<"Amplifying voxel region z=["<<split_vals[i]<<","<<split_vals[i+1]<<"] "<<i<<"/"<<split_vals.size()-1-1<<endl;
                    
                    // THIS IS THE COMPUTATIONALLY EXPENSIVE PART
                    for_each(indices.begin(), indices.end(), [&](int ihit) {
                        int xx = floor((S3D_x[ihit]- xmin)/ xbin_dim);
                        int yy = floor((S3D_y[ihit]- ymin)/ ybin_dim);
                        int zz = floor((S3D_z[ihit]-split_vals[i])/ zbin_dim);
                        hc[xx][yy][zz] += 1.;
                    });
                    auto endcriti = std::chrono::steady_clock::now();
                    dur_criti=dur_criti+endcriti-startcriti;
                    

                    long int LL = (x_n_bin)*(y_n_bin)*(z_n_bin);
                    long int not_empty=0;
                    
                    auto startampli = std::chrono::steady_clock::now();
                    // Applying GEM3 amplification
                    
                    
                    
                    for(int xx = 0; xx<x_n_bin-1; xx++){
                        for(int yy=0; yy<y_n_bin-1; yy++) {
                            for(int zz=0; zz<z_n_bin-1; zz++){
                                if(hc[xx][yy][zz] != 0.) {
                                    not_empty++;
                                    // optbeta is multiplied by factors to normalize on volume chosen
                                    hout[xx][yy]+=Nph_saturation(hc[xx][yy][zz], optA, optbeta*xy_vox_scale*xy_vox_scale*0.1/zbin_dim);
                                }
                            }
                        }
                    }
                    auto endampli = std::chrono::steady_clock::now();
                    dur_ampli=dur_ampli+endampli-startampli;
                    cout<<"Sparse-ness of voxel region: "<< not_empty * 100./LL<<" %"<<endl;
                }
            }
            
        }

        cout<<"size = "<<size_tot<<endl;
        std::cout << "Time smear " << dur_smear.count() << " seconds" <<std::endl;
        std::cout << "Time Critical " << dur_criti.count() << " seconds" <<std::endl;
        std::cout << "Time ampli " << dur_ampli.count() << " seconds" <<std::endl;
        
        // Applying camera response + Poisson smearing
        for_each(hout.begin(), hout.end(),[&](std::vector<int>& v)  {
            transform (v.begin(), v.end(), v.begin(), [&] (int elem){
                return gRandom->Poisson(elem *
                                        omega *
                                        optphotons_per_el *
                                        optcounts_per_photon);
            });
        });       
        
        // Padding
        // FIXME: Write a function padding()
        //Define a translation vector
        
        // DEBUG
        //int sommatotale =0;
        //
        //sommatotale = accumulate(hout.cbegin(), hout.cend(), 0, [](auto sum, const auto& row) {
        //            return accumulate(row.cbegin(), row.cend(), sum);
        //        });
        //
        //cout<<"INT = "<<sommatotale<<endl;

        int x_center_cloud=(int)round(((xmax+xmin)/2.)/xbin_dim);
        int y_center_cloud=(int)round(((ymax+ymin)/2.)/ybin_dim);

        //DEBUG
        //cout<<"cc   = ("<<x_center_cloud<<","<<y_center_cloud<<")"<<endl;
        vector<int> translation = {x_center_cloud, y_center_cloud};
        // Calculate the center position of the original array in the padded array
        vector<int> center = {(int)(static_cast<double>(x_pix)*xy_vox_scale/2.)+translation[0],
                              (int)(static_cast<double>(y_pix)*xy_vox_scale/2.)+translation[1]
                             };
                             
        // DEBUG
        //cout<<"c   = ("<<center[0]<<","<<center[1]<<")"<<endl;
        
        // cout<<"Center: "<<center[0]<<", "<<center[1]<<endl;
        int x_start = max(0, center[0] -    (int)hout.size()/2);
        int y_start = max(0, center[1] - (int)hout[0].size()/2);
        int x_end   = min(x_pix*xy_vox_scale, x_start + (int)hout.size());
        int y_end   = min(y_pix*xy_vox_scale, y_start + (int)hout[0].size());
        // cout<<"PADDING ["<<x_start<<":"<<x_end<<","<<y_start<<":"<<y_end<<"]"<<endl;
        
        for(int xx=x_start; xx<x_end; xx++){
            for(int yy=y_start; yy<y_end; yy++){
                array2d_Nph[xx/xy_vox_scale][yy/xy_vox_scale]+=hout[xx-x_start][yy-y_start];
            }
        }
        
    }
    return;
}

void compute_cmos_without_saturation(vector<double>& x_hits_tr,
                                     vector<double>& y_hits_tr,
                                     vector<double>& z_hits_tr,
                                     vector<double>& energy_hits,
                                     map<string,string>& options,
                                     vector<vector<double>>& array2d_Nph) {
    
    vector<vector<double>> signal(x_pix,
                                  vector<double>(y_pix, 0.0));
    
    vector<float> S2D_x;
    vector<float> S2D_y;
    ph_smearing2D(x_hits_tr,
                  y_hits_tr,
                  z_hits_tr,
                  energy_hits,
                  options,
                  S2D_x,
                  S2D_y
                  );
    
    // Vector to store all indices
    vector<size_t> indices(S2D_x.size());
    // Fill indices with 0, 1, 2, ..., numbers.size() - 1
    iota(indices.begin(), indices.end(), 0);
    
    double optx_dim = stod(options["x_dim"]);
    double optx_pix = static_cast<double>(x_pix);
    double opty_dim = stod(options["y_dim"]);
    double opty_pix = static_cast<double>(y_pix);
    
    // THIS IS THE COMPUTATIONALLY EXPENSIVE PART
    for_each(indices.begin(), indices.end(), [&](int ihit) {
        int xx = floor((0.5 * optx_dim + S2D_x[ihit]) * optx_pix / optx_dim);
        int yy = floor((0.5 * opty_dim + S2D_y[ihit]) * opty_pix / opty_dim);
        signal[xx][yy] += 1.;
    });
    
    // DEBUG
    //double ntot =0.;
    //for(unsigned int xx = 0; xx<signal.size(); xx++) {
    //    for(unsigned int yy = 0; yy<signal[0].size(); yy++) {
    //        ntot+=signal[xx][yy];
    //    }
    //}
    //cout<<"Tot num of sensor counts after GEM3 without saturation: "<<ntot<<endl;
    
    array2d_Nph = signal;
    
    return;
}

void cloud_smearing3D(vector<double>& x_hits_tr,
                      vector<double>& y_hits_tr,
                      vector<double>& z_hits_tr,
                      vector<double>& energy_hits,
                      map<string,string>& options,
                      vector<float>& S3D_x,
                      vector<float>& S3D_y,
                      vector<float>& S3D_z) {

    vector<double> nel = NelGEM2(energy_hits, z_hits_tr, options);
    //DEBUG
    //for(unsigned int i=0; i<nel.size(); i++) {
    //    cout<<nel[i]<<"\n";
    //}
    
    vector<double> dz;
    int opt_gem=stod(options["z_extra"]);
    transform(z_hits_tr.begin(),z_hits_tr.end(),back_inserter(dz), [&] (double a) { return a+opt_gem;});

    vector<double> sigma_x = compute_sigma(stod(options["diff_const_sigma0T"]), stod(options["diff_coeff_T"]), dz);
    vector<double> sigma_y = compute_sigma(stod(options["diff_const_sigma0T"]), stod(options["diff_coeff_T"]), dz);
    vector<double> sigma_z = compute_sigma(stod(options["diff_const_sigma0L"]), stod(options["diff_coeff_L"]), dz);

    //Here this is the slowest part
    //Sequential
    /*S3D_x = smear(x_hits_tr, sigma_x, nel);
    S3D_y = smear(y_hits_tr, sigma_y, nel);
    S3D_z = smear(z_hits_tr, sigma_z, nel);*/
    //Parallel
    smear_parallel(x_hits_tr,y_hits_tr,z_hits_tr,sigma_x,sigma_y,sigma_z,nel,S3D_x,S3D_y,S3D_z);
    // DEBUG
    //for(unsigned int i=0; i<S3D_x.size(); i++) {
    //    cout<<S3D_x[i]<<endl;
    //}

    return;
}

void ph_smearing2D(vector<double>& x_hits_tr,
                   vector<double>& y_hits_tr,
                   vector<double>& z_hits_tr,
                   vector<double>& energy_hits,
                   map<string,string>& options,
                   vector<float>& S2D_x,
                   vector<float>& S2D_y) {
    
    // Electrons in GEM2
    vector<double> nel = NelGEM2(energy_hits, z_hits_tr, options);
    
    double optphotons_per_el    = stod(options["photons_per_el"]);
    double optA                 = stod(options["A"]);
    // Photons in GEM3 (the factor A is added to be able to compare saturated and non-saturated results)
    vector<double> nph;
    transform(nel.begin(), nel.end(), back_inserter(nph), [&](double nel_i) {
        return nel_i * optA * GEM3_gain * omega * optphotons_per_el * optcounts_per_photon;
    });
    
    vector<double> dz;
    int opt_gem=stod(options["z_extra"]);
    transform(z_hits_tr.begin(),z_hits_tr.end(),back_inserter(dz), [&] (double a) { return a+opt_gem;});
    
    vector<double> sigma_xy = compute_sigma(stod(options["diff_const_sigma0T"]), stod(options["diff_coeff_T"]), dz);
    
    S2D_x = smear(x_hits_tr, sigma_xy, nph);
    S2D_y = smear(y_hits_tr, sigma_xy, nph);
    
    return;
}

vector<double> compute_sigma(const double diff_const, const double diff_coeff, const vector<double>& dz) {
    vector<double> sigmas;
    transform(dz.begin(), dz.end(), back_inserter(sigmas), [&] (double a) {
        return sqrt(diff_const + diff_coeff * a /10.0);
    });
    return sigmas;
}
    
vector<float> smear(const vector<double>& axis_hit, const vector<double>& axis_sigma, const vector<double>& nel) {

    long int nelsum = accumulate(nel.begin(), nel.end(), (long int)0);
    
    vector<float> X;
    X.reserve(nelsum);
    
    // Create a vector of indices where each index i is repeated nel[i] times
    vector<long int> indices(nelsum);
    vector<long int> positions(axis_hit.size() + 1, 0);
    
    // Compute cumulative sum of nel to determine positions
    partial_sum(nel.begin(), nel.end(), positions.begin() + 1);

    // Fill the indices vector
    for_each(positions.begin(), positions.end() - 1, [&, i = 0](long int pos) mutable {
        fill(indices.begin() + pos, indices.begin() + positions[i + 1], i);
        ++i;
    });
    
    // Fill X with Gaussian-distributed values based on axis_hit and axis_sigma
    //This here is the slowest part
    transform(indices.begin(), indices.end(), back_inserter(X), [&](int i) {
        return (float)gRandom->Gaus(axis_hit[i], axis_sigma[i]);
    });
    
    return X;
}

void smear_parallel(const vector<double>& x_axis_hit,const vector<double>& y_axis_hit,const vector<double>& z_axis_hit,const vector<double>& x_axis_sigma,const vector<double>& y_axis_sigma,const vector<double>& z_axis_sigma,const vector<double>& nel,vector<float>& X,vector<float>& Y,vector<float>& Z)
{
    long int nelsum = accumulate(nel.begin(), nel.end(), (long int)0);
    
    X.reserve(nelsum);
    Y.reserve(nelsum);
    Z.reserve(nelsum);
    
    // Create a vector of indices where each index i is repeated nel[i] times
    vector<long int> indices(nelsum);
    vector<long int> positions(x_axis_hit.size() + 1, 0);
    
    // Compute cumulative sum of nel to determine positions
    partial_sum(nel.begin(), nel.end(), positions.begin() + 1);

    // Fill the indices vector
    for_each(positions.begin(), positions.end() - 1, [&, i = 0](long int pos) mutable {
        fill(indices.begin() + pos, indices.begin() + positions[i + 1], i);
        ++i;
    });
    
    // Fill X with Gaussian-distributed values based on axis_hit and axis_sigma
    //This here is the slowest part
    auto startstep4 = std::chrono::steady_clock::now();

    tbb::spin_mutex myMutex;
    // Run the default parallelism
    tbb::task_arena arena(num_threads);
    arena.execute([&]{
        parallel_for(tbb::blocked_range<size_t>(0, indices.size(),indices.size()/num_threads),
            [&] (auto &range) {
            vector<float> x_paralvec,y_paralvec,z_paralvec;
            TRandom3 paralrandom;
            myMutex.lock();
            paralrandom.SetSeed(floor(gRandom->Rndm()*10000));
            myMutex.unlock();
            for(auto iterator=range.begin();iterator<range.end();++iterator)
            {
                int index = indices[iterator];
                x_paralvec.push_back((float)paralrandom.Gaus(x_axis_hit[index], x_axis_sigma[index]));
                y_paralvec.push_back((float)paralrandom.Gaus(y_axis_hit[index], y_axis_sigma[index]));
                z_paralvec.push_back((float)paralrandom.Gaus(z_axis_hit[index], z_axis_sigma[index]));
            }
            //cout<<"Mutex\n";
            //cout << "Thread ID: " << this_thread::get_id() << endl;
            //tbb::queuing_mutex::scoped_lock myLock(myMutex);
            tbb::spin_mutex::scoped_lock myLock(myMutex);	//maybe better to use oneapi::tbb::spin_mutex to use spi_mutex since the critical operation is small
            //merge of paralvec in X
            X.insert(X.end(),x_paralvec.begin(),x_paralvec.end());
            Y.insert(Y.end(),y_paralvec.begin(),y_paralvec.end());
            Z.insert(Z.end(),z_paralvec.begin(),z_paralvec.end());
        });
    });    

    auto endstep4 = std::chrono::steady_clock::now();
    //cout<<X.size()<<endl;
    //cout<<Y.size()<<endl;
    //cout<<Z.size()<<endl;

    //std::chrono::duration<double> dur=endstep4-startstep4;
    //std::cout << "Slowest Time smear part " << dur.count() << " seconds" <<std::endl;

    return ;
}

vector<double> arange(double start, double stop, double step) {
    
    int length = (stop - start) / step;
    vector<double> result(length+1);
    double value = start;
    generate(result.begin(), result.end(), [&value, step]() mutable {
        double current = value;
        value += step;
        return current;
    });
    return result;
}

double round_up_to_even(const double f) {
    return ceil(f / 2.0) * 2.0;
}

double Nph_saturation(int nel, double A, double beta) {
    return nel * A * GEM3_gain / (1.0 + beta * GEM3_gain * nel);
}


void TrackVignetting(vector<vector<double>>& arrTr, int xpix, int ypix, const TH2F & VignMap) {
    
    for(int xx = 0; xx < xpix; xx++) {
        for(int yy = 0; yy < ypix; yy++) {
            if(arrTr[xx][yy] != 0) {
                arrTr[xx][yy]=round(arrTr[xx][yy] *
                                    VignMap.GetBinContent(VignMap.GetXaxis()->FindBin(xx),
                                                          VignMap.GetYaxis()->FindBin(yy)
                                                          )
                                    );
            }
        }
    }
    
    return;
}

// Old approach:
//string rootlocation(string tag, int run){
//    string sel;
//    if (tag == "Data") {
//        if ((run>=936) && (run<=1601)) {
//            sel = "Data/LTD/Data_Camera/ROOT";
//        } else if ((run>=1632) && (run<4505)) {
//            sel = "Data/LAB";
//        } else if ((run>=4470) && (run<10000)) {
//            sel = "LAB";
//        } else {
//            cerr<<"rootlocation: Error: Data taken with another DAQ or not yet uploaded to the cloud"<<endl;
//            exit(EXIT_FAILURE);
//        }
//    } else if (tag == "DataMango") {
//        sel = (run<3242) ? "Data/Man" : "MAN";
//    } else if (tag ==  "MC") {
//        sel = "Simulation";
//        cerr<<"rootlocation: Error: automatic download for Simulated data not implemented yet"<<endl;
//        exit(EXIT_FAILURE);
//    }
//
//    return sel;
//}

//string root_filename(string tag, int run) {
//
//    string sel = rootlocation(tag,run);
//
//    string BASE_URL = "https://s3.cloud.infn.it/v1/AUTH_2ebf769785574195bde2ff418deac08a/";
//    string bucket;
//    if(tag.find("MC") != std::string::npos) {
//        bucket = (tag=="MC-old") ? "cygnus" : "cygno-sim";
//    } else if (tag == "Data") {
//        bucket = (run<4505) ? "cygnus" : "cygno-data";
//    } else if (tag == "DataMango") {
//        bucket = (run<3242) ? "cygnus" : "cygno-data";
//    }
//
//    BASE_URL = Form("%s%s/", BASE_URL.c_str(), bucket.c_str());
//    string file_root = Form("%s/histogr_Run%05d.root", sel.c_str(), run);
//
//    return Form("%s%s", BASE_URL.c_str(), file_root.c_str());
//}
//
