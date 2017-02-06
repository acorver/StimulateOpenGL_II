
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

#include "Starfield.h"

#define PI 3.14159265359

/* ============================================================
* Init
* ========================================================= */

Starfield::Starfield() : StimPlugin("Starfield")
{
}

Starfield::~Starfield()
{
	cleanup();
}

void Starfield::cleanup()
{
	for (std::vector<Starfield::Star>::iterator it =
		m_vShapes.begin(); it != m_vShapes.end(); it++)
	{
		delete (*it).pQ;
	}
	m_vShapes.clear();

	m_vTrials.clear();
}

bool Starfield::init()
{
	// Make sure all previous stars are removed
	cleanup();

	// Load parameters
	float out;
	if (getParam("bgcolor"         , out)) { bgcolor              = out; } else { bgcolor              = 1.0;  }
	if (getParam("objcolor"        , out)) { paramObjColor        = out; } else { paramObjColor        = 0.0;  }
	if (getParam("density"         , out)) { paramDensity         = out; } else { paramDensity         = 1e-4; }
	if (getParam("skyRadius"       , out)) { paramSkyRadius       = out; } else { paramSkyRadius       = 1e3;  }
	if (getParam("starRadius"      , out)) { paramStarRadius      = out; } else { paramStarRadius      = 5;    }
	if (getParam("trialsAzimuth"   , out)) { paramTrialsAzimuth   = out; } else { paramTrialsAzimuth   = 20;   }
	if (getParam("trialsElevation" , out)) { paramTrialsElevation = out; } else { paramTrialsElevation = 10;   }
	if (getParam("trialsVelocity"  , out)) { paramTrialsVelocity  = out; } else { paramTrialsVelocity  = 3;    }
	if (getParam("trialDuration"   , out)) { paramTrialDuration   = out; } else { paramTrialDuration   = 4;    }
	if (getParam("trialRepetition" , out)) { paramTrialRepetition = out; } else { paramTrialRepetition = 2;    }
	if (getParam("trialMaxVel"     , out)) { paramTrialMaxVel     = out; } else { paramTrialMaxVel     = 10;   }
	if (getParam("trialMinVel"     , out)) { paramTrialMinVel     = out; } else { paramTrialMinVel     = 40;   }
	if (getParam("starGrid"        , out)) { paramStarGrid        = out; } else { paramStarGrid        = 0;    }
	if (getParam("camPosZ"         , out)) { paramCamPosZ         = out; } else { paramCamPosZ         = 0;    }
	if (getParam("wireframe"       , out)) { paramWireFrame       = out; } else { paramWireFrame       = 0;    }

	// Initialize variables
	m_vAxisOfRotation = Vec3(1, 0, 0);
	m_TimeSinceLastTrial = 1e9;
	nSubFrames = 1;
	if (fps_mode == FPS_Triple) { nSubFrames = 3; } else 
	if (fps_mode == FPS_Dual  ) { nSubFrames = 2; } else 
	if (fps_mode == FPS_Single) { nSubFrames = 1; }

	// Initialize FrameVars file
	frameVars->setVariableNames(QString(
		"frameNum angle angularVelocity axisX axisY axisZ").split(" "));
	frameVars->setVariableDefaults(QVector<double>() << 
		-1 << 0 << 0 << 0 << 0 << 0 );

	// Create the stars randomly (but evenly distributed on the sphere)
	//    See: https://www.cmu.edu/biolphys/deserno/pdf/sphere_equi.pdf
	if (paramStarGrid != 1)
	{
		int numStars = 4 * PI * paramSkyRadius * paramSkyRadius * paramDensity;

		for (int i = 0; i < numStars; i++)
		{
			double z   = -paramSkyRadius + 2 * paramSkyRadius * double(qrand()) / double(RAND_MAX);
			double phi = 2 * PI * double(qrand()) / double(RAND_MAX);
			double x = sqrt(paramSkyRadius*paramSkyRadius - z*z) * cos(phi);
			double y = sqrt(paramSkyRadius*paramSkyRadius - z*z) * sin(phi);

			Starfield::Star s;

			s.pos.x = x;
			s.pos.z = y;
			s.pos.y = z;

			s.pQ = gluNewQuadric();
			gluQuadricNormals(s.pQ, GLU_SMOOTH);

			if (paramWireFrame != 1)
			{
				gluQuadricDrawStyle(s.pQ, GLU_FILL);
			} else {
				gluQuadricDrawStyle(s.pQ, GLU_LINE);
			}
			gluQuadricOrientation(s.pQ, GLU_OUTSIDE);

			m_vShapes.push_back(s);
		}
	}
	// create the stars deterministically (note these are not evenly distributed)
	else 
	{
		for (double a = 0; a < PI; a += PI / sqrt(1/paramDensity))
		{
			for (double p = -PI; p < PI; p += 2 * PI / sqrt(1 / paramDensity))
			{
				Starfield::Star s;

				s.pos.x = cos(a) * cos(p) * paramSkyRadius;
				s.pos.z = sin(a) * cos(p) * paramSkyRadius;
				s.pos.y = sin(p) * paramSkyRadius;

				s.pQ = gluNewQuadric();
				gluQuadricNormals(s.pQ, GLU_SMOOTH);
				gluQuadricDrawStyle(s.pQ, GLU_FILL);
				gluQuadricOrientation(s.pQ, GLU_OUTSIDE);

				m_vShapes.push_back(s);
			}
		}
	}

	// Initialize trials
	initTrials();
	
	// Invalidate timer
	m_dLastTime = std::numeric_limits<double>::min();

	return true;
}

/* ============================================================
* Draw frame
* ========================================================= */

void Starfield::drawFrame()
{
	for (m_nCurSubframe = 0; m_nCurSubframe < nSubFrames; m_nCurSubframe++)
	{
		update();
		doFrameDraw();

		// Order: frameNum angle angularVelocity axisX axisY axisZ
		frameVars->enqueue(QVector<double>() <<
			double(frameNum) << double(m_Angle) << double(m_AngularVelocity) << 
			double(m_vAxisOfRotation.x) << double(m_vAxisOfRotation.y) << 
			double(m_vAxisOfRotation.z) );
	}

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void Starfield::doFrameDraw()
{
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	
	// Set camera
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluPerspective(45.0f, width() / height(), 0.01f, 1000.0f);
	
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	gluLookAt(0, 0, 0, 0, 0, 100, 0, 1, 0);
	glTranslatef(0, 0, -paramCamPosZ);
	glRotatef(m_Angle, m_vAxisOfRotation.x, m_vAxisOfRotation.y, m_vAxisOfRotation.z);

	// Set color
	if (fps_mode == FPS_Triple || fps_mode == FPS_Dual) {
		// order of frames comes from config parameter 'color_order' but defaults to RGB
		if      ( m_nCurSubframe == b_index) { glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE); }
		else if ( m_nCurSubframe == r_index) { glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE); }
		else if ( m_nCurSubframe == g_index) { glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE); }
	}
	glColor3f(paramObjColor, paramObjColor, paramObjColor);

	// Draw scene
	Starfield::Star st;
	for (std::vector<Starfield::Star>::iterator it =
		m_vShapes.begin(); it != m_vShapes.end(); it++)
	{
		st = *it;
		glPushMatrix();
		glTranslatef(st.pos.x, st.pos.y, st.pos.z);
		gluSphere(st.pQ, paramStarRadius, 8, 8);
		glPopMatrix();
	}

	// Revert matrices and settings, so that other plugins are unaffected
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}

/* ============================================================
* Initialize trials
* ========================================================= */

void Starfield::initTrials()
{
	for (int repetition = 0; repetition < paramTrialRepetition; repetition++) 
	{
		std::vector<Starfield::Trial> vTrials;

		for (double a = 0.0; a < PI; a += PI / paramTrialsAzimuth)
		{
			for (double e = -PI; e < PI; e += 2 * PI / paramTrialsElevation)
			{
				for (double s = paramTrialMinVel; s <= paramTrialMaxVel; s +=
					(paramTrialMaxVel - paramTrialMinVel) / paramTrialsVelocity)
				{
					Starfield::Trial trial;
					trial.vAxisOfRotation.x = cos(a) * cos(e);
					trial.vAxisOfRotation.z = sin(a) * cos(e);
					trial.vAxisOfRotation.y = sin(e);
					trial.dAngularVelocity = s;
					vTrials.push_back(trial);
				}
			}
		}

		std::random_shuffle(vTrials.begin(), vTrials.end());
		m_vTrials.insert(m_vTrials.end(), vTrials.begin(), vTrials.end());
	}
}

/* ============================================================
* Update frame / change trials
* ========================================================= */

void Starfield::update()
{
	// Compute elapsed time
	double curTime = getTime();
	double elapsedTime = curTime - m_dLastTime;
	m_dLastTime = curTime;

	// Only update if m_dLastTime was valid (minimum of 10 FPS, i.e. no sudden jumps)
	if (elapsedTime < 0.1)
	{
		// Update trial
		m_TimeSinceLastTrial += elapsedTime;
		if (m_TimeSinceLastTrial > paramTrialDuration)
		{
			m_TimeSinceLastTrial = 0.0;
			m_Angle = 0.0;
			m_vTrials.pop_back();

			// Log new trial
			Log() << "Started new Starfield trial: vel=" << m_vTrials.back().dAngularVelocity << ",axis=(" <<
				m_vTrials.back().vAxisOfRotation.x << "," << m_vTrials.back().vAxisOfRotation.y << "," <<
				m_vTrials.back().vAxisOfRotation.z << ")\n";
		}

		// Update angle and axis of rotation
		m_AngularVelocity = m_vTrials.back().dAngularVelocity;
		m_Angle += elapsedTime * m_AngularVelocity;
		m_vAxisOfRotation = m_vTrials.back().vAxisOfRotation;
	}
}

/* ============================================================
* Save framevar file
* ========================================================= */

void Starfield::afterFTBoxDraw()
{
	if (frameVars && frameVars->queueCount())
	{
		frameVars->commitQueue();
	}
}
