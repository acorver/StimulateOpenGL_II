/*
*  Starfield.cpp
*  StimulateOpenGL_II
*
*  Author: Abel Corver
*          abel.corver@gmail.com
*          (Anthony Leonardo Lab, Janelia Research Campus, HHMI)
*
*  Created on: February, 2017
*
*/

#ifndef Starfield_H
#define Starfield_H

#include "StimPlugin.h"
#include "Shapes.h"

class Starfield : public StimPlugin
{
	friend class GLWindow;
	Starfield();

public:
	virtual ~Starfield();

	// Density of "stars" in stars/solid angle
	double paramDensity;
	double paramSkyRadius;
	double paramStarRadius;
	int    paramTrialsAzimuth;
	int    paramTrialsElevation;
	double paramTrialDuration;
	int    paramTrialRepetition;
	double paramTrialMaxVel;
	double paramTrialMinVel;
	int    paramTrialsVelocity;
	int    paramStarGrid;
	double paramCamPosZ;
	double paramObjColor;
	int    nSubFrames;


protected:
	bool init();
	void cleanup();
	void drawFrame();
	void afterFTBoxDraw();

private:
	struct Star {
		GLUquadric* pQ;
		Vec3 pos;
	};

	struct Trial {
		Vec3    vAxisOfRotation;
		double  dAngularVelocity;
	};

	// Initialize/update the trials
	void initTrials();
	void update();

	// Draw the actual graphics
	void doFrameDraw();

	// Rendering information
	std::vector<Star> m_vShapes;
	double m_dLastTime;
	int m_nCurSubframe;

	// Current starfield orientation / velocities
	double m_Angle;
	double m_AngularVelocity;
	Vec3   m_vAxisOfRotation;

	// Trial information
	std::vector<Starfield::Trial> m_vTrials;
	float  m_TimeSinceLastTrial;
};

#endif