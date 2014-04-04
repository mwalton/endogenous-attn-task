#ifndef PRP_DEVICE_H
#define PRP_DEVICE_H

#include <vector>
#include <string>

#include <iostream>
#include <fstream>
#include <sstream>

#include "EPICLib/Device_base.h"
#include "EPICLib/Symbol.h"
#include "EPICLib/Geometry.h"
#include "Statistics.h"

namespace GU = Geometry_Utilities;
using namespace std;
using std::ostringstream;

// see implementation file for conversion functions and constants 
// pertaining to screen layout

class simple_device : public Device_base {
public:
	simple_device(const std::string& id, Output_tee& ot);
			
	virtual void initialize();
	virtual void set_parameter_string(const std::string&);
	virtual std::string get_parameter_string() const;

	virtual void handle_Start_event();
	virtual void handle_Stop_event();
	
	virtual void handle_Delay_event(const Symbol& type, const Symbol& datum, 
		const Symbol& object_name, const Symbol& property_name, const Symbol& property_value);
	virtual void handle_Keystroke_event(const Symbol& key_name);
			
private:
	enum task_type_e {Easy, Hard}; //there were made generic so that this .h file could be generic to all prp
    enum trial_validity_e {Valid, Invalid};
	
	task_type_e task_type;
    trial_validity_e trial_validity;
	
	enum State_e {START, START_TRIAL, REMOVE_CUE, PRESENT_STIMULUS, WAITING_FOR_RESPONSE, DISCARD_STIMULUS, SHUTDOWN};
	
	State_e state;
	
	// parameters
	int n_total_trials;	 //total trials in the run
	int colorcount = 4; //number of colors to display
    double percent_invalid;
	std::string tagstr; //for any info, defaults to "draft"
	
	
	// stimulus and response lists
	std::vector<Symbol> vstims;  //list of visual stimului
	std::vector<Symbol> vresps;  //list of visual responses (same order as stimuli)
	std::vector<double> vlocs; //list of x offsets for vstim location

	// data states
	Symbol cstim_v_name;  //visual warning stim name on each trial

	Symbol vstim_name;    //visual stimulus name on each trial
	Symbol vstim_text;    //actual visual stimulus [UNUSED]
	Symbol vstim_color;	  //actual visual stimulus
	Symbol correct_vresp; //current correct response for visual stimulus
    Symbol valid_cue;

	int trial;                 //curent trial number
	bool vresponse_made;       //whether visual response has been made
	double vstim_xloc;	//holds x offset of vstim
	int stimwaittime;			//time of blank between fixation and stim onset
	
	Current_mean current_vrt;
	
	bool reparse_conditionstring; //used for when task is restarted after a halt
	
	// data accumulation		
	std::string condition_string; //holds current condition
	int n_trials;                 //number of trials run so far
	long vstim_onset;             //timestamp for visual stimulus onset
	std::vector <std::string> prspathvector;
	std::string prsfilenameonly;
	
	ostringstream outputString;
	ostringstream DataOutputString;
	std::string trial_data_string;
	std::ofstream dataoutput_stream;				// Output data on every trail	
			
	// helpers
	void parse_condition_string();
	void present_cue();
	void remove_cue();
	void start_trial();
	void present_stimulus();
	void remove_stimulus();
	void setup_next_trial();
	void stop_experiment();
	void make_vis_stim_appear(); //dissappears are handled by response event handlers

	void refresh_experiment(); //tls -> cleans up run vars so that you can re-run after experiment completes
	
	void output_statistics(); //const;
	void show_message(const std::string& thestring, const bool addendl = false);
	void openOutputFile(ofstream & outFileStream, const string filename_text);
	bool fexists(const char *filename);
	void stringsplit(std::string str, std::string delim, vector<std::string> results);
	void clear_prspathvector();
	std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
	std::vector<std::string> split(const std::string &s, char delim);
	
	// rule out copy, assignment
	simple_device(const simple_device&);
	simple_device& operator= (const simple_device&);
};

#endif


