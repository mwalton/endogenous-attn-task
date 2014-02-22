/**********************************************************************
  mhpchoice task device
  Simple choice RT task
  5/4/09 - Travis Seymour
**********************************************************************/
/**********************************************************************
trial start
Visual Warning
Stimulus (red or blue circle, Y position is 0, X posistion randomly selected from [-1, -0.5, 0, 0.5, 1]
{button press --> j button for red, f button for blue}
Cleanup For Next Trial

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
const Symbol Warning_c("##");
const Symbol VWarn_c("Fixation");
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
        n_total_trials(0), condition_string("10 Easy"), task_type(Easy), n_trials(0), trial(0), vresponse_made(false),
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
	error_msg += "\n Should be: space-delimited trials(int > 0) task_type(Easy, Hard)";
	istringstream iss(condition_string);
	
	int nt;
	string tt;
	iss >> nt >> tt;
	// do all error checks before last read
	if(!iss)
		throw Device_exception(this, string("Incorrect condition string: ") + error_msg);
	if(nt <= 0)
		throw Device_exception(this, string("Number of trials must be positive: ") + error_msg);
	if(!(tt == "Easy" || tt == "easy" || tt == "Hard" || tt == "Hard"))	
		throw Device_exception(this, string("Unrecognized task type specification: ") + error_msg);
	//assign parameters
	if(tt == "Easy" || tt == "easy")
		task_type = Easy; //congruent
	else if(tt == "Hard" || tt == "hard")
		task_type = Hard; //incongruent
	n_trials = nt;
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
	if(task_type == Easy) {
		
		//fill stimulus vector
		vstims.clear();
		vstims.push_back(Blue_c);
		vstims.push_back(Red_c);
		vstims.push_back(Blue_c);
		vstims.push_back(Red_c);		
		
		//fill response vector
		vresps.clear();
		vresps.push_back(Symbol("F"));	// right index
		vresps.push_back(Symbol("J"));	// right middle
		vresps.push_back(Symbol("F"));	// right ring
		vresps.push_back(Symbol("J"));	// right little
	} 
	else { 
		
		//fill stimulus vector
		vstims.clear();
		vstims.push_back(Blue_c);
		vstims.push_back(Red_c);
		vstims.push_back(Green_c);
		vstims.push_back(Yellow_c);		
		
		vresps.clear();
		vresps.push_back(Symbol("F"));	// right index
		vresps.push_back(Symbol("J"));	// right middle
		vresps.push_back(Symbol("D"));	// right ring
		vresps.push_back(Symbol("K"));	// right little
	}
	
	//identify the experiment in the device and trace output
	if(device_out) {
		device_out << "Device: MHP Choice Task v1.0" << endl;
		device_out << "Conditions: " << condition_string << endl;
	}
	
	DataOutputString.str("");
	
	reparse_conditionstring = false;	// in the current version of EPICX, the Device_base subobject has not yet been 
	// connected to a Device_processor, which is necessary for the get_trace() function
	// to work correctly - a null pointer access is the result. Since this happens
	// at startup time, the result is confusing, but visible in the debugger.
	// This will probably be fixed in the newer version of the framework.
	// DK
/*	if(get_trace() && Trace_out) {
		Trace_out << "Device: MHP Detection Task v1.0" << endl;
		Trace_out << "Conditions: " << condition_string << endl;
	}
	*/	
}

void simple_device::handle_Start_event()
{
//	if(device_out)
//		device_out << processor_info() << "received Start_event" << endl;
// You have to get the ball rolling with a first time-delayed event - nothing happens until you do.
// DK
 	schedule_delay_event(500);
}

//called after the stop_simulation function (which is bart of the base device class)
void simple_device::handle_Stop_event()
{
//	if(device_out)
//		device_out << processor_info() << "received Stop_event" << endl;
	
	//show final stats. Not sure if it's best to call this here, or have
	//  the keystroke handler do it (as VPTS_device did).
	output_statistics();	
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
			//Trial Start Detected. Show Warning Stimuli, Set State For Stim1 in 500 ms
			vresponse_made = false;
			start_trial();
			state = PRESENT_STIMULUS;
			schedule_delay_event(1000);
			break;
		case PRESENT_STIMULUS: 
			if (show_states) show_message("********-->STATE: PRESENT_STIMULUS",true);
			//Signal for Stim Display. Show Stim, then either wait for response
			present_stimulus();
			//if(task_type == DTeasy || task_type == DThard) {
			state = WAITING_FOR_RESPONSE;
			break;
		case WAITING_FOR_RESPONSE:
			if (show_states) show_message("********-->STATE: WAITING_FOR_RESPONSE",true);
			//nothing to do, just wait
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
	
	//present visual warning 
	wstim_v_name = concatenate_to_Symbol(VWarn_c, trial);
	make_visual_object_appear(wstim_v_name, wstim_location_c, wstim_size_c);
	set_visual_object_property(wstim_v_name, Text_c, Warning_c);
	set_visual_object_property(wstim_v_name, Color_c, Black_c);
	
	if (show_debug) show_message("trial_start*", true);
}

void simple_device::present_stimulus()
{
	// remove the warningstimulus
	//***THIS SHOULD HAVE IT'S OWN EVENT!!
	make_visual_object_disappear(wstim_v_name);
	
	//show the stimulus
	make_vis_stim_appear();
}

void simple_device::make_vis_stim_appear()
{
	if (show_debug) show_message("*make_vis_stim_appear|");
	int stim_index = random_int(4);				// chooses one of the vstims to display
	//vstim_text = vstims.at(stim_index);
	vstim_color = vstims.at(stim_index);
	correct_vresp = vresps.at(stim_index);
	vstim_name = concatenate_to_Symbol(VStim_c, trial);
	
	//randomly choose xoffset
	int loc_index = random_int(5) + 1;
	switch(loc_index) {
		case 1:
			vstim_xloc = 0.;
			break;
		case 2:
			vstim_xloc = 0.5;
			break;
		case 3:
			vstim_xloc = 1.;
			break;
		case 4:
			vstim_xloc = -0.5;
			break;	
		case 5:
			vstim_xloc = -1.0;
			break;
	}
	
	//make_visual_object_appear(vstim_name, vstim_location_c, vstim_size_c);
	make_visual_object_appear(vstim_name, GU::Point(vstim_xloc,0.), vstim_size_c);
	set_visual_object_property(vstim_name,Shape_c, Circle_c);
	//set_visual_object_property(vstim_name, Text_c, Warning_c);
    set_visual_object_property(vstim_name, Color_c, vstim_color);
	
	if (vstim_xloc < 0) 
		set_visual_object_property(vstim_name, Left_of_c, wstim_v_name);
	else if (vstim_xloc > 0) 
		set_visual_object_property(vstim_name, Right_of_c, wstim_v_name);
	else 
		set_visual_object_property(vstim_name, In_center_of_c, wstim_v_name);
	
	vstim_onset = get_time();
	vresponse_made = false;
	if (show_debug) show_message("make_vis_stim_appear*", true);
}
	

void simple_device::setup_next_trial()
{
	if (show_debug) show_message("*setup_next_trial|");	

	// set up another trial if the experiment is to continue
	if(trial < n_trials) {
		state = START_TRIAL;
		schedule_delay_event(intertrialinterval_c);
		}
	else 
		state = SHUTDOWN;
		schedule_delay_event(500);
		//stop_simulation();
	if (show_debug) show_message("setup_next_trial*");
}


// here if a keystroke event is received
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
		DataOutputString << trial << "," << rt << "," << vstim_color << "," << vstim_xloc << "," << key_name << "," << correct_vresp << "," << "CORRECT" << endl;
	}
	else {
		//throw Device_exception(this, string("Unrecognized keystroke: ") + key_name.str());
		long rt = get_time() - vstim_onset;
		//if(trial > 1) current_vrt.update(rt); //don't average incorrect responses 
		outputString.str("");
		outputString << "Trial #" << trial << " | (choicetask) | RT: " << rt << " | Stimulus: " << vstim_color << " | Keystroke: " << key_name << " | CorrectResponse:" << correct_vresp << " | (INCORRECT)" << endl;
		DataOutputString << trial << "," << rt << "," << vstim_color << "," << vstim_xloc << "," << key_name << "," << correct_vresp << "," << "INCORRECT" << endl;
	}
	show_message(outputString.str());
	vresponse_made = true;
	// remove the stimulus
	make_visual_object_disappear(vstim_name);
			
	setup_next_trial();
	if (show_debug) show_message("....handle_Keystroke_event*");
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
			outputString << "MHP Choice (Easy)";
			break;
		case Hard: 
			outputString << "MHP Choice (Hard)";
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
	outputString << "\ntrial,rt,vstim_color,vstim_xloc,key_name,correct_vresp,accuracy" << endl;
	show_message(outputString.str());
	show_message(DataOutputString.str());
}

void simple_device::show_message(const std::string& thestring, const bool addendl) {

	if (get_trace() && Trace_out) Trace_out << thestring;
	if (device_out) device_out << thestring;

	if (addendl) {
		if (get_trace() && Trace_out) Trace_out << endl;
		if (device_out) device_out << endl;
	}
}

