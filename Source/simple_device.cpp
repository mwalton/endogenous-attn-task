/**********************************************************************
  mhpchoice task device
  Simple choice RT task
  Jan 9 2010 - Travis Seymour
**********************************************************************/


#include "simple_device.h"
#include "Statistics.h"
#include "EPICLib/Geometry.h"
#include "EPICLib/Output_tee_globals.h"
#include "EPICLib/Output_tee.h"
#include "EPICLib/Numeric_utilities.h"
#include "EPICLib/Symbol_utilities.h"
#include "EPICLib/Assert_throw.h"
#include "EPICLib/Device_exception.h"
#include "EPICLib/Standard_symbols.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <algorithm>
#include <vector>
//#include <cassert>
#include <cmath>

namespace GU = Geometry_Utilities;
//using GU::Point;
using namespace std;
using std::ostringstream;

// handy internal constants
const Symbol Cue_Left_c("<");
const Symbol Cue_Rigth_c(">");
const Symbol VCue_c("Cue");
const Symbol VStim_c("Stimulus");

//const Symbol Right_of_c("Right_of");
//const Symbol Left_of_c("Left_of");
//const Symbol In_center_of_c("In_center_of_c");
	
// experiment constants
const GU::Point wstim_location_c(0., 0.);
const GU::Size wstim_size_c(1., 1.);
//const GU::Point vstim_location_c(1., 0.);
const GU::Size vstim_size_c(1., 1.);
const long intertrialinterval_c = 5000;

const bool show_debug = true;
const bool show_states = false;

simple_device::simple_device(const std::string& device_name, Output_tee& ot) :
		Device_base(device_name, ot), 
        n_total_trials(0), condition_string("10 0 Easy Draft"), task_type(Easy), percent_invalid(0), n_trials(0), trial(0), vresponse_made(false), tagstr("Draft"),
	state(START) //should this be in initialize? (tls)
{
	// parse condition string and initialize the task
	parse_condition_string();		
	initialize();
}


void simple_device::parse_condition_string()
// modified by ajh to add the second parameter
{
	// build an error message string in case we need it
	string error_msg(condition_string);
	error_msg += "\n Should be: space-delimited trials(int > 0) proportion of invalid cues(1-4) difficulty(Easy or Hard)";
	istringstream iss(condition_string);
	
	int nt;
	int pi;
	string tt;
	string ts;
	iss >> nt >> pi >> tt >> ts;
	// do all error checks before last read
	if(!iss)
		throw Device_exception(this, string("Incorrect condition string: ") + error_msg);
	if(nt <= 0)
		throw Device_exception(this, string("Number of trials must be positive ") + error_msg);
	if((pi < 0) || (pi > 1))
		throw Device_exception(this, string("Proportion of invalid cues must be between 0 and 1 ") + error_msg);
	if(!(tt == "Easy" || tt == "easy" || tt == "Hard" || tt == "Hard"))	
		throw Device_exception(this, string("Task difficulty must be 'Easy' or 'Hard'") + error_msg);
	//assign parameters
	if (tagstr != "") tagstr = ts;
	percent_invalid = pi;
	n_trials = nt;
	if(tt == "Easy" || tt == "easy")
		task_type = Easy; //all stim at fixation
	else if(tt == "Hard" || tt == "hard")
		task_type = Hard; //stim eccentricity varies
}

void simple_device::set_parameter_string(const string& condition_string_)
{
	condition_string = condition_string_;
	parse_condition_string();
}

string simple_device::get_parameter_string() const
{
	return condition_string;
}

void simple_device::initialize()
{
	vresponse_made = false;
	trial = 0;
	state = START;
	current_vrt.reset();	
	
		
		//fill stimulus vector
		vstims.clear();
		vstims.push_back(Red_c);
		vstims.push_back(Green_c);
		vstims.push_back(Blue_c);
		vstims.push_back(Yellow_c);		
		
		vresps.clear();
		vresps.push_back(Symbol("U"));	// right index
		vresps.push_back(Symbol("I"));	// right middle
		vresps.push_back(Symbol("O"));	// right ring
		vresps.push_back(Symbol("P"));	// right little

	
	//identify the experiment in the device and trace output
    //TODO make parameter output appropriate for task
	if(device_out) {
		device_out << "******************************************" << endl;
		device_out << "Initializeing Device: Exogenous Attention Task v0.1" << endl;
		device_out << "Conditions: " << condition_string << endl;
		device_out << "******************************************" << endl;
	
		device_out << "**********************************************************************" << endl;
		device_out << " trial start" << endl;
		device_out << " Visual Cue '<' or '>' (1000ms)" << endl;
		device_out << " Blank Screen (500-1300ms)" << endl;
		device_out << " Stimulus (Red,Green,Blue, or Yellow) circle, " << endl;
		device_out << " {button press --> U, I, O, or P}" << endl;
		device_out << " Cleanup For Next Trial (ITI = 5000ms)" << endl;
		device_out << "";
		device_out << " Conditions" << endl;
		device_out << " ----------" << endl;
		device_out << " Easy: X position is at fixation" << endl;
		device_out << " Hard: X posistion randomly selected from [-2, -1, 0, 1, 2] deg rel to fixation" << endl;
		device_out << " ----------" << endl;
		device_out << " [note, now a 4th parameter that can be any string, write it to output with data.]" << endl;
		device_out << "**********************************************************************" << endl;		
		
	}
	
	DataOutputString.str("");
	
	// If the streams were open, close.
	// This is just incase the model is re-initialized.
	if(dataoutput_stream.is_open())
	{
		dataoutput_stream.close();
	}	
	
	// open the data output stream for overwriting!
	openOutputFile(dataoutput_stream, string("data_output"));
	//dataoutput_stream << endl;
	//dataoutput_stream << outputString << "\nTASKTYPE,DIFFICULTY,TRIAL,RT,STIMCOLOR,STIMLOC,RESPONSE,CORRECTRESPONSE,ACCURACY,NUMCOLORS,STIMWAITTIME" << endl;	
	
	reparse_conditionstring = false;	
	
	// in the current version of EPICX, the Device_base subobject has not yet been 
	// connected to a Device_processor, which is necessary for the get_trace() function
	// to work correctly - a null pointer access is the result. Since this happens
	// at startup time, the result is confusing, but visible in the debugger.
	// This will probably be fixed in the newer version of the framework.
	// DK
	/*	
	 if(get_trace() && Trace_out) {
	 Trace_out << "Device: MHP Choice Task v3.0" << endl;
	 Trace_out << "Conditions: " << condition_string << endl;
	 }
	 */		
}

// You have to get the ball rolling with a first time-delayed event - nothing happens until you do.
// DK
void simple_device::handle_Start_event()
{
	//	if(device_out)
	//		device_out << processor_info() << "received Start_event" << endl;

	//determine rule file name so that it can be added to data output. 
	prsfilenameonly = "";
	device_out << "@@RuleFile[Full]: " << prsfilename << endl;
	clear_prspathvector();	//clear out prspathvector
	prspathvector = split(prsfilename, '/'); //fill prspathvector with the rule file path
	if (prspathvector.size() > 0) { 
		prsfilenameonly = prspathvector[prspathvector.size() - 1];
	}
	else {
		prsfilenameonly = "?????.prs";
	}
	device_out << "@@RuleFile[NameOnly]: " << prsfilenameonly << endl;
	
	if(device_out) {
		device_out << "******************{{{{{{{{{{{{{{{{__SIMULATION_START__}}}}}}}}}}}}}}}}***************************" << endl;
		device_out << "******************{{{{{{{{{{{{{{{{__SIMULATION_START__}}}}}}}}}}}}}}}}***************************" << endl;
	}
	
 	schedule_delay_event(500);
}

//called after the stop_simulation function (which is bart of the base device class)
void simple_device::handle_Stop_event()
{
	//	if(device_out)
	//		device_out << processor_info() << "received Stop_event" << endl;
	
	//show final stats. 	
	output_statistics();
	
	refresh_experiment();
	
	//close data output file
	if(dataoutput_stream.is_open())
	{
		dataoutput_stream.close();
	}		
}

void simple_device::handle_Delay_event(const Symbol& type, const Symbol& datum, 
		const Symbol& object_name, const Symbol& property_name, const Symbol& property_value)
{	
	switch(state) {
		case START:
			if (show_states) show_message("********-->STATE: START",true);
			//Block Start Detected. Set State to Trial Start and Wait 500 ms
			state = START_TRIAL;
			schedule_delay_event(500);
			break;
		case START_TRIAL:
			if (show_states) show_message("********-->STATE: START_TRIAL",true);
			//Trial Start Detected. Show Warning Stimuli, Set State For Warning Removal in 500 ms
			vresponse_made = false;
			start_trial();
			state = REMOVE_CUE;
			schedule_delay_event(1000);
			break;
		case REMOVE_CUE:
			if (show_states) show_message("********-->STATE: REMOVE_FIXATION",true);
			//Remove The Fixation, wait 500-1300ms, Show Stimulus
			remove_cue();
			state = PRESENT_STIMULUS;
			stimwaittime = random_int(800) + 500;
			schedule_delay_event(stimwaittime);
			break;
		case PRESENT_STIMULUS: 
			if (show_states) show_message("********-->STATE: PRESENT_STIMULUS",true);
			//Signal for Stim Display. Show Stim, then either wait for response
			present_stimulus();
			state = WAITING_FOR_RESPONSE;
			break;
		case WAITING_FOR_RESPONSE:
			if (show_states) show_message("********-->STATE: WAITING_FOR_RESPONSE",true);
			//nothing to do, just wait
			//**note: next state and schedule_delay set by response handler
			break;
		case DISCARD_STIMULUS:
			if (show_states) show_message("********-->STATE: REMOVING_STIMULUS",true);
			//remove the stimulus
			remove_stimulus();
			break;
		case SHUTDOWN:
			if (show_states) show_message("********-->STATE: SHUTDOWN",true);
			//Detected Signal to Stop Simulation.
			stop_simulation();
			break;
		default:
			throw Device_exception(this, "Device delay event in unknown or improper device state");
			break;
		}
}

//At Trial Start, Warning Stimuli are presented
void simple_device::start_trial()
{
	if (show_debug) show_message("*trial_start|");
	
	//only occurs if task is stopped and restarted
	if (reparse_conditionstring == true) {
		parse_condition_string(); 
		initialize();
	}
	
	trial++; //increment trial counter
	
	present_cue();
	
	if (show_debug) show_message("trial_start*", true);
}

void simple_device::present_cue()
{
	
	if (show_debug) show_message("*present_cue|");
    
    int left_or_right = rand() % 2;
    switch (left_or_right) {
        case 0:
            valid_cue = Cue_Left_c;
            break;
        default:
            valid_cue = Cue_Rigth_c;
            break;
    }
	
	//display visual fixation piont 
	cstim_v_name = concatenate_to_Symbol(VCue_c, trial);
	make_visual_object_appear(cstim_v_name, wstim_location_c, wstim_size_c);
	set_visual_object_property(cstim_v_name, Text_c, valid_cue);
	set_visual_object_property(cstim_v_name, Color_c, Black_c);
	
	if (show_debug) show_message("present_cue*", true);
}

void simple_device::remove_cue()
{
	if (show_debug) show_message("*remove_cue|");
	
	// remove the warningstimulus
	make_visual_object_disappear(cstim_v_name);
	
	if (show_debug) show_message("remove_cue*", true);
}

void simple_device::present_stimulus()
{

	//show the stimulus
	make_vis_stim_appear();
}

void simple_device::make_vis_stim_appear()
{
	if (show_debug) show_message("*make_vis_stim_appear|");
	int stim_index = random_int(1);				// chooses one of the vstims to display
	vstim_color = vstims.at(stim_index);
	correct_vresp = vresps.at(stim_index);
	vstim_name = concatenate_to_Symbol(VStim_c, trial);
	
    double rand_val = (double)rand() / RAND_MAX;
    bool isValid;
    
    isValid = (rand_val < percent_invalid);
    
	switch(task_type) {
		case Easy: 
			if (valid_cue == Cue_Left_c) vstim_xloc = -3.;
            else vstim_xloc = 3.;
			break;
		case Hard: 
			if (valid_cue == Cue_Left_c) vstim_xloc = -7.;
            else vstim_xloc = 7.;
			break;
	}
	
	make_visual_object_appear(vstim_name, GU::Point(vstim_xloc,0.), vstim_size_c);
	set_visual_object_property(vstim_name,Shape_c, Square_c);
    set_visual_object_property(vstim_name, Color_c, vstim_color);
	
	/*
	if (vstim_xloc < 0) 
		set_visual_object_property(vstim_name, Left_of_c, wstim_v_name);
	else if (vstim_xloc > 0) 
		set_visual_object_property(vstim_name, Right_of_c, wstim_v_name);
	else 
		set_visual_object_property(vstim_name, In_center_of_c, wstim_v_name);
	*/
	 
	vstim_onset = get_time();
	vresponse_made = false;
	if (show_debug) show_message("make_vis_stim_appear*", true);
}


// here if a keystroke event is received
//TODO: make compatable w/ attn model
void simple_device::handle_Keystroke_event(const Symbol& key_name)
{
	if (show_debug) show_message("*handle_Keystroke_event....",true);
	//ostringstream outputString;  //defined in simple_device.h (tls)
	outputString.str("");
	
	if(key_name == correct_vresp) {
		long rt = get_time() - vstim_onset;
		if(trial > 1) current_vrt.update(rt);
		outputString.str("");
		outputString << "Trial #" << trial << " | (choicetask) | RT: " << rt << " | Stimulus: " << vstim_color << " | Keystroke: " << key_name << " | CorrectResponse:" << correct_vresp << " | (CORRECT)" << endl;
		DataOutputString << "MHPCHOICETASK" << "," << task_type << "," << trial << "," << rt << "," << vstim_color << "," << vstim_xloc << "," << key_name << "," << correct_vresp << "," << "CORRECT" << "," << "fixme:COLORCNT_PLACEHOLDER" << "," << stimwaittime << "," << tagstr << "," << prsfilenameonly << endl;
	}
	else {
		//throw Device_exception(this, string("Unrecognized keystroke: ") + key_name.str());
		long rt = get_time() - vstim_onset;
		//if(trial > 1) current_vrt.update(rt); //don't average incorrect responses 
		outputString.str("");
		outputString << "Trial #" << trial << " | (choicetask) | RT: " << rt << " | Stimulus: " << vstim_color << " | Keystroke: " << key_name << " | CorrectResponse:" << correct_vresp << " | (INCORRECT)" << endl;
		DataOutputString << "MHPCHOICETASK" << "," << task_type << "," << trial << "," << rt << "," << vstim_color << "," << vstim_xloc << "," << key_name << "," << correct_vresp << "," << "INCORRECT" << "," << "fixme:COLORCNT_PLACEHOLDER" << "," << stimwaittime << "," << tagstr << "," << prsfilenameonly << endl;
	}
	show_message(outputString.str());
	vresponse_made = true;
	
	if (show_debug) show_message("....handle_Keystroke_event*");
	
	state = DISCARD_STIMULUS;
	schedule_delay_event(500);
	
	
}

void simple_device::remove_stimulus() 
{
	if (show_debug) show_message("*removing_stimulus|");
	
	// remove the stimulus
	make_visual_object_disappear(vstim_name);
	
	setup_next_trial();
	if (show_debug) show_message("....removing_stimulus*");
}

void simple_device::setup_next_trial()
{	
	// set up another trial if the experiment is to continue
	if(trial < n_trials) {
		if (show_debug) show_message("*setup_next_trial|");
		state = START_TRIAL;
		schedule_delay_event(intertrialinterval_c);
		if (show_debug) show_message("setup_next_trial*");
	}
	else { 
		if (show_debug) show_message("*shutdown_experiment|");
		state = SHUTDOWN;
		schedule_delay_event(500);
		//stop_simulation();
		if (show_debug) show_message("shutdown_experiment*");
	}
}

void simple_device::refresh_experiment() {
	// call this from very last procedure...currently that is output_statistics()
	
	vresponse_made = false;
	trial = 0;
	state = START;
	current_vrt.reset();
	reparse_conditionstring = true;
}

void simple_device::output_statistics() //const
{
	if (show_debug) show_message("*output_statistics|");
	
	show_message("*** End of experiment! ***",true);

	// show condition
	outputString.str("");
	outputString << "\nCONDITION = ";
	switch(task_type) {
		case Easy: 
			outputString << "MHP Choice Task Easy (Stim location always at fixation)";
			break;
		case Hard: 
			outputString << "MHP Choice Task Hard (Stim location varies [0,1,-1,2,or -2 deg from fixation])";
			break;
	}
	show_message(outputString.str());	
	
	// show total trials
	outputString.str("");
	outputString << "\nTotal trials = " << '\t' << n_trials << endl;
	show_message(outputString.str());

	// show performance
	outputString.str("");
	outputString << "N = " << setw(3) << current_vrt.get_n() 
			<< ", RT = " << fixed << setprecision(0) << setw(4) << current_vrt.get_mean();
	show_message(outputString.str(),true);
	show_message(" ", true);
	show_message("NOTE: Average Ignores 1st Trial",true);
					
	show_message("*** ****************** ***",true);
	
	refresh_experiment();

	if (show_debug) show_message("output_statistics*",true);
	
	show_message("************* RAW DATA ***************");
	outputString.str("");
	outputString << "\nTASKTYPE,DIFFICULTY,TRIAL,RT,STIMCOLOR,STIMLOC,RESPONSE,CORRECTRESPONSE,ACCURACY,NUMCOLORS,STIMWAITTIME,TAG,RULES" << endl;
	show_message(outputString.str());
	show_message(DataOutputString.str());
	show_message("**************************************");
	
	//Write Collated Raw Data To Output File
	dataoutput_stream << DataOutputString.str();	
}

void simple_device::show_message(const std::string& thestring, const bool addendl) {

	if (get_trace() && Trace_out) Trace_out << thestring;
	if (device_out) device_out << thestring;

	if (addendl) {
		if (get_trace() && Trace_out) Trace_out << endl;
		if (device_out) device_out << endl;
	}
}

void simple_device::openOutputFile(ofstream & outFileStream, const string filename_text)

{
	string fileName = filename_text + ".csv";
	bool filealreadyexists = fexists(fileName.c_str());
	//outFileStream.open((fileName).c_str(), ofstream::out); //overwriting
	outFileStream.open((fileName).c_str(), ofstream::app); //appending
	if(!outFileStream.is_open()) {
		throw Device_exception(this, " Error opening output file: " + fileName);
		show_message("Error opening output file:" + fileName, true);
	} 
	else if (!filealreadyexists) 
		outFileStream << "\nTASKTYPE,DIFFICULTY,TRIAL,RT,STIMCOLOR,STIMLOC,RESPONSE,CORRECTRESPONSE,ACCURACY,NUMCOLORS,STIMWAITTIME,TAG,RULES" << endl;
	
}

bool simple_device::fexists(const char *filename)
{
	ifstream ifile(filename);
	return ifile;
}

//------------------------------------------------------------------------------
// Split string by a delim. The first puts the results in an already constructed 
// vector, the second returns a new vector. Note that this solution does not skip 
// empty tokens, so the following will find 4 items, one of which is empty:
// std::vector<std::string> x = split("one:two::three", ':');
//------------------------------------------------------------------------------
std::vector<std::string> &simple_device::split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while(std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
} 

std::vector<std::string> simple_device::split(const std::string &s, char delim) {
	std::vector<std::string> elems;     
	return split(s, delim, elems); 
}


void simple_device::clear_prspathvector() {  
	vector<std::string>().swap(prspathvector);  
}
