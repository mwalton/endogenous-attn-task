#ifndef STATISTICS_H
#define STATISTICS_H

#include <EPICLib/Geometry.h>
namespace GU = Geometry_Utilities;


/*
Use these classes to accumulate running statistics values easily.
reset() - zero the internal variables
update() - add a new data value, updating current average
get_n, get_rms/mean - return the current values
*/

class Current_rms_error {
public:
	Current_rms_error()
		{
			reset();
		}
	void reset()
		{
			n = 0;
			total = 0.;
			rms = 0.;
		}
	
	double update(GU::Point p1, GU::Point p2);
			
	int get_n() const
		{return n;}
	double get_rms() const
		{return rms;}
			
private:
	int n;
	double total;
	double rms;
};


class Current_mean{
public:
	Current_mean()
		{
			reset();
		}
	void reset()
		{
			n = 0;
			total = 0.;
			mean = 0.;
		}
	
	int get_n() const
		{return n;}
	double get_mean() const
		{return mean;}
			
	double update(double x)
		{
			total += x;
			n++;
			mean = total / n;
			return mean;
		}
			
private:
	int n;
	double total;
	double mean;
};




#endif
