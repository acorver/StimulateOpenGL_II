#include "MovingObjects.h"
#include "Shapes.h"
#include "GLHeaders.h"
#include <math.h>

#define DEFAULT_TYPE BoxType
#define DEFAULT_LEN 8
#define DEFAULT_VEL 4
#define DEFAULT_POS_X 400
#define DEFAULT_POS_Y 300
#define DEFAULT_POS_Z 0
#define DEFAULT_BGCOLOR 1.0
#define DEFAULT_OBJCOLOR 0.0
#define DEFAULT_TFRAMES 120*60*60*10 /* 120fps*s */
#define DEFAULT_JITTERMAG 2
#define DEFAULT_RSEED -1
#define NUM_FRAME_VARS 10
#define DEFAULT_MAX_Z 1000


MovingObjects::MovingObjects()
    : StimPlugin("MovingObjects"), savedrng(false)
{
}

MovingObjects::~MovingObjects()
{
}

QString MovingObjects::objTypeStrs[] = { "box", "ellipsoid" };

MovingObjects::ObjData::ObjData() : shape(0) { initDefaults(); }

void MovingObjects::ObjData::initDefaults() {
	if (shape) delete shape, shape = 0;
	type = DEFAULT_TYPE, jitterx = 0., jittery = 0., jitterz = 0., 
	phi_o = 0.;
	spin = 0.;
	vel_vec_i = -1;
	len_vec_i = 0;
	len_vec.resize(1);
	vel_vec.resize(1);
	len_vec[0] = Vec2(DEFAULT_LEN, DEFAULT_LEN);
	vel_vec[0] = Vec3(DEFAULT_VEL,DEFAULT_VEL,0.0);
	
	pos_o = Vec3(DEFAULT_POS_X,DEFAULT_POS_Y,DEFAULT_POS_Z);
	v = vel_vec[0], vel = vel_vec[0];
	color = DEFAULT_OBJCOLOR;
}

bool MovingObjects::init()
{
	
	initCameraDistance();

	bool haveSphere = false;
	
    moveFlag = true;

	// set up pixel color blending to enable 480Hz monitor mode

	// set default parameters
	// basic target attributes
	objs.clear();
	
	if (!getParam("numObj", numObj)
		&& !getParam("numObjs", numObj) ) numObj = 1;
	
	int numSizes = -1, numSpeeds = -1;

	fvHasPhiCol = fvHasZCol = fvHasZScaledCol = false;
	didScaledZWarning = 0;
	
	for (int i = 0; i < numObj; ++i) {
		if (i > 0)	paramSuffixPush(QString::number(i+1)); // make the additional obj params end in a number
       	objs.push_back(!i ? ObjData() : objs.front()); // get defaults from first object
		ObjData & o = objs.back();
			
		// if any of the params below are missing, the defaults in initDefaults() above are taken
		
		QString otype; 
		if (getParam( "objType"     , otype)) {
			otype = otype.trimmed().toLower();
			
			if (otype == "ellipse" || otype == "ellipsoid" || otype == "circle" || otype == "disk" || otype == "sphere"
				|| otype == "1") {
				if (otype == "sphere") //Warning() << "`sphere' objType not supported, defaulting to ellipsoid.";
					o.type = SphereType, haveSphere = true;
				else
					o.type = EllipseType;
			} else
				o.type = BoxType;
		}
		
		QVector<double> r1, r2;
		// be really tolerant with object maj/minor length variable names 
		getParam( "objLenX" , r1)
		|| getParam( "rx"          , r1) 
		|| getParam( "r1"          , r1) 
	    || getParam( "objLen"   , r1)
		|| getParam( "objLenMaj", r1)
		|| getParam( "objLenMajor" , r1)
		|| getParam( "objMajorLen" , r1)
		|| getParam( "objMajLen" , r1)
		|| getParam( "xradius"   , r1);
		getParam( "objLenY" , r2)
		|| getParam( "ry"          , r2)
		|| getParam( "r2"          , r2)
		|| getParam( "objLenMinor" , r2) 
		|| getParam( "objLenMin"   , r2)
		|| getParam( "objMinorLen" , r2)
		|| getParam( "objMinLen"   , r2)
		|| getParam( "yradius"   , r2);
		
		if (!r2.size()) r2 = r1;
		if (r1.size() != r2.size()) {
			Error() << "Target size vectors mismatch for object " << i+1 << ": Specify two comma-separated lists (of the same length):  objLenX and objLenY, to create the targetSize vector!";
			return false;
		}
		o.len_vec.resize(r1.size());
		for (int j = 0; j < r1.size(); ++j)
			o.len_vec[j] = Vec2(r1[j], r2[j]);
		
		if (!o.len_vec.size()) {
			if (i) o.len_vec = objs.front().len_vec; // grab the lengths from predecessor if not specified
			else { o.len_vec.resize(1); o.len_vec[0] = Vec2Zero; }
		}
		if (numSizes < 0) numSizes = o.len_vec.size();
		else if (numSizes != o.len_vec.size()) {
			Error() << "Object " << i+1 << " has a lengths vector of size " << o.len_vec.size() << " but size " << numSizes << " was expected. All objects should have the same-sized lengths vector!";
			return false;
		}
		
		getParam( "objSpin"     , o.spin);
		getParam( "objPhi" , o.phi_o) || getParam( "phi", o.phi_o );  
		QVector<double> vx, vy, vz;
		getParam( "objVelx"     , vx); 
		getParam( "objVely"     , vy); 
		getParam( "objVelz"     , vz); 
		if (!vy.size()) vy = vx;
		if (vx.size() != vy.size()) {
			Error() << "Target velocity vectors mismatch for object " << i+1 << ": Specify two comma-separated lists (of the same length):  objVelX and objVelY, to create the targetVelocities vector!";
			return false;			
		}
		if (vz.size() != vx.size()) {
			vz.clear();
			vz.fill(0., vx.size());
		}
		o.vel_vec.resize(vx.size());
		for (int j = 0; j < vx.size(); ++j) {
			o.vel_vec[j] = Vec3(vx[j], vy[j], vz[j]);
		}
		if (!o.vel_vec.size()) {
			if (i) o.vel_vec = objs.front().vel_vec;
			else { o.vel_vec.resize(1); o.vel_vec[0] = Vec3Zero; }
		}
		if (numSpeeds < 0) numSpeeds = o.vel_vec.size();
		else if (numSpeeds != o.vel_vec.size()) {
			Error() << "Object " << i+1 << " has a velocities vector of size " << o.vel_vec.size() << " but size " << numSpeeds << " was expected. All objects should have the same-sized velocities vector!";
			return false;
		}
		o.vel = o.vel_vec[0];
		o.v = Vec3Zero;
		getParam( "objXinit"    , o.pos_o.x); 
		getParam( "objYinit"    , o.pos_o.y); 
		bool hadZInit = getParam( "objZinit"    , o.pos_o.z);
		double zScaled;
		if (getParam( "objZScaledinit"    , zScaled)) {
			if (hadZInit) Warning() << "Object " << i << " had objZInit and objZScaledinit params specified, using the zScaled param and ignoring zinit.";
			o.pos_o.z = distanceToZ(zScaled);
		}
		getParam( "objcolor"    , o.color);
				
		paramSuffixPop();
		
	}

	// note bgcolor already set in StimPlugin, re-default it to 1.0 if not set in data file
	if(!getParam( "bgcolor" , bgcolor))	     bgcolor = 1.;

	// trajectory stuff
	if(!getParam( "rndtrial" , rndtrial))	     rndtrial = 0;
		// rndtrial=0 -> repeat traj every tframes; 
		// rndtrial=1 new start point and speed every tframes; start=rnd(mon_x_pix,mon_y_pix); speed= random +-objVelx, objVely
		// rndtrial=2 random position and direction, static velocity
	    // rndtrial=3 keep old position of last tframes run, random velocity based on initialization range
		// rndtrial=4 keep old position of last tframes run, static speed, random direction
	if(!getParam( "rseed" , rseed))              rseed = -1;  //set start point of rnd seed;
        ran1Gen.reseed(rseed);
	
	bool dontInitFvars (false);
	
	// if we are looping the plugin, then we continue the random number generator state
	if (savedrng && rndtrial) {
		ran1Gen.reseed(saved_ran1state);
		Debug() << ".. continued RNG with seed " << saved_ran1state;
		dontInitFvars = true;
	}
	
	if(!getParam( "tframes" , tframes) || tframes <= 0) tframes = DEFAULT_TFRAMES, ftChangeEvery = -1; 
	if (tframes <= 0 && (numSpeeds > 1 || numSizes > 1)) {
		Error() << "tframes needs to be specified because either the lengths vector or the speeds vector has length > 1!";
		return false;	
	}
	
	if(!getParam( "jitterlocal" , jitterlocal))  jitterlocal = false;
	if(!getParam( "jittermag" , jittermag))	     jittermag = DEFAULT_JITTERMAG;
	if (!getParam("jitterInZ", jitterInZ)) jitterInZ = false;

	if (!getParam("ft_change_frame_cycle", ftChangeEvery) && !getParam("ftrack_change_frame_cycle",ftChangeEvery) 
		&& !getParam("ftrack_change_cycle",ftChangeEvery) && !getParam("ftrack_change",ftChangeEvery)) 
		ftChangeEvery = 0; // override default for movingobjects it is 0 which means autocompute
	

	if (!(getParam("wrapEdge", wrapEdge) || getParam("wrap", wrapEdge))) 
			wrapEdge = false;

	if (!getParam("debugAABB", debugAABB))
			debugAABB = false;

	// Light position stuff...
	if (!(getParam("sharedLight", lightIsFixedInSpace) || getParam("fixedLight", lightIsFixedInSpace) || getParam("lightIsFixed", lightIsFixedInSpace) || getParam("lightIsFixedInSpace", lightIsFixedInSpace)))
		lightIsFixedInSpace = true;	
	QVector<double> tmpvec;
	if ((getParam("lightPosition", tmpvec) || getParam("lightPos", tmpvec)) && tmpvec.size() >= 3) {
		for (int i = 0; i < 3; ++i) lightPos[i] = tmpvec[i];
		lightPos[2] = -lightPos[2];  // invert Z: OpenGL coords have positive z towards camera and we are inverse of that
	} else {
		lightPos = Vec3(width(), height(), 100.0);
	}
	lightIsDirectional = !lightIsFixedInSpace;
	
	initObjs();
	
	QString dummy;
	if (getParam("targetcycle", dummy) || getParam("speedcycle", dummy)) {
		Error() << "targetcycle and speedcycle params are no longer supported!  Instead, pass a comma-separated-list for the velocities and object sizes!";
		return false;
	}

	if ( getParam( "max_x_pix" , dummy) || getParam( "min_x_pix" , dummy) 
		|| getParam( "max_y_pix" , dummy) || getParam( "min_y_pix" , dummy) ) {
		Error() << "min_x_pix/max_x_pix/min_y_pix/max_y_pix no longer supported in MovingObjects.  Use the lmargin,rmargin,bmargin,tmargin params instead!";
		return false;
	}
	// these affect bounding box for motion
	max_x_pix = width() - rmargin;
	min_x_pix = lmargin;
	min_y_pix = bmargin;
	max_y_pix = height() - tmargin;
	
    // the object area AABB -- a rect constrained by min_x_pix,min_y_pix and max_x_pix,max_y_pix
	canvasAABB = Rect(Vec2(min_x_pix, min_y_pix), Vec2(max_x_pix-min_x_pix, max_y_pix-min_y_pix)); 

	//if (!dontInitFvars) {
		ObjData o = objs.front();
		double x = o.pos_o.x, y = o.pos_o.y, r1 = o.len_vec.front().x, r2 = o.len_vec.front().y;
		frameVars->setVariableNames(   QString(          "frameNum objNum subFrameNum objType(0=box,1=ellipse) x y r1 r2 phi color z zScaled").split(QString(" ")));
		frameVars->setVariableDefaults(QVector<double>()  << 0     << 0   << 0        << o.type                << x
									                                                                             << y
									                                                                               << r1
									                                                                                  << r2
									                                                                                    << o.phi_o
									                                                                                         << o.color 
																																   << 0.0
																																	   << 1.0);
		frameVars->setPrecision(10, 9);
		frameVars->setPrecision(11, 9);
		
	//}
	
	
	if (ftChangeEvery == 0 && tframes > 0) {
		Log() << "Auto-set ftrack_change variable: FT_Change will be asserted every " << tframes << " frames.";		
		ftChangeEvery = tframes;
	} else if (ftChangeEvery > 0 && tframes > 0 && ftChangeEvery != tframes) {
		Warning() << "ftrack_change was defined in configuration and so was tframes (target cycle / speed cycle) but ftrack_change != tframes!";
	}
	if (!have_fv_input_file && int(nFrames) != tframes * numSizes * numSpeeds) {
		int oldnFrames = nFrames;
		nFrames = tframes * numSpeeds * numSizes;
		Warning() << "nFrames was " << oldnFrames << ", auto-set to match tframes*length(speeds)*length(sizes) = " << nFrames << "!";
	}
	
	dontCloseFVarFileAcrossLoops = bool(rndtrial && nFrames);

	// NB: the below is a performance optimization for Shapes such as Ellipse and Rectangle which create 1 display list per 
	// object -- the below ensures that the shared static display list is compiled after init is done so that we don't have to compile one later
	// while the plugin is running
	Shapes::InitStaticDisplayLists();
	
	if (!softCleanup) glGetIntegerv(GL_SHADE_MODEL, &savedShadeModel);
	if (haveSphere) glShadeModel(GL_SMOOTH);
	
	if (!getParam("maxZ", maxZ)) maxZ = DEFAULT_MAX_Z;
	if (maxZ < 0.0) {
		Error() << "Specified maxZ value is too small -- it needs to be positive nonzeo!";
		return false;
	}

	return true;
}

void MovingObjects::initObj(ObjData & o) {
	if (o.type == EllipseType) {
		o.shape = new Shapes::Ellipse(o.len_vec[0].x, o.len_vec[0].y);
	} else if (o.type == SphereType) {
		Shapes::Sphere *sph = 0;
		o.shape = sph = new Shapes::Sphere(o.len_vec[0].x);
		sph->lightIsFixedInSpace = lightIsFixedInSpace;
		for (int i = 0; i < 3; ++i)	sph->lightPosition[i] = lightPos[i]*2.0;
		sph->lightPosition[3] = lightIsDirectional ? 0.0f : 1.0f;
	} else {  // box, etc
		o.shape = new Shapes::Rectangle(o.len_vec[0].x, o.len_vec[0].y);
	}
	o.shape->position = o.pos_o;
	o.shape->noMatrixAttribPush = true; ///< performance hack	
}

void MovingObjects::initObjs() {
	int i = 0;
	for (QList<ObjData>::iterator it = objs.begin(); it != objs.end(); ++it, ++i) {
		ObjData & o = *it;
		initObj(o);
		if (i < savedLastPositions.size())
			o.lastPos = savedLastPositions[i];
	}
}

void MovingObjects::cleanupObjs() {
	savedLastPositions.clear();
	if (softCleanup) savedLastPositions.reserve(objs.size());
	for (QList<ObjData>::iterator it = objs.begin(); it != objs.end(); ++it) {
		ObjData & o = *it;
		if (softCleanup) savedLastPositions.push_back(o.lastPos);
		delete o.shape, o.shape = 0;
	}	
	objs.clear();
	numObj = 0;
}



void MovingObjects::cleanup()
{
	if ((savedrng = softCleanup)) {
		saved_ran1state = ran1Gen.currentSeed();		
	}
    cleanupObjs();
	if (!softCleanup) {
		Shapes::CleanupStaticDisplayLists();
		glShadeModel(savedShadeModel);
	}
}

bool MovingObjects::processKey(int key)
{
    switch ( key ) {
    case 'm':
    case 'M':
        moveFlag = !moveFlag;
        return true;
	case 'j':
	case 'J':
		jitterlocal = !jitterlocal;
		return true;
    }
    return StimPlugin::processKey(key);
}

void MovingObjects::drawFrame()
{
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ); 
        
    if (fps_mode != FPS_Single) {
		glEnable(GL_BLEND);
		
		if (bgcolor >  0.5)
			glBlendFunc(GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR);
		else glBlendFunc(GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR);
    }

	doFrameDraw();
   
    if (fps_mode != FPS_Single) {
		if (bgcolor >  0.5)
			glBlendFunc(GL_DST_COLOR,GL_ONE);
        else glBlendFunc(GL_SRC_COLOR,GL_ONE);

		glDisable(GL_BLEND);
    }

}

void MovingObjects::doFrameDraw()
{
	int objNum = 0;
	for (QList<ObjData>::iterator it = objs.begin(); it != objs.end(); ++it, ++objNum) {		
		ObjData & o = *it;
		if (!o.shape) continue; // should never happen..		
		double & x  (o.shape->position.x),					 
		       & y  (o.shape->position.y),	
			   & z  (o.shape->position.z),
		       & vx (o.v.x),
		       & vy (o.v.y),
			   & vz (o.v.z),
		       & objVelx (o.vel.x),
		       & objVely (o.vel.y),
			   & objVelz (o.vel.z);
		double d = o.shape->distance();		
		Vec2 cpos (o.shape->canvasPosition()), ///< position as rendered on the canvas after distance calc
			 cvel (vx/d,vy/d); ///< velocity as apparent on the canvas after distance calc
		
		const double
		       & objXinit (o.pos_o.x),
			   & objYinit (o.pos_o.y),
		       & objZinit (o.pos_o.z),
		       & cx (cpos.x),
		       & cy (cpos.y),
			   & cvx (cvel.x),
		       & cvy (cvel.y);
		
		const double  & objLen_o (o.len_vec[0].x), & objLen_min_o (o.len_vec[0].y);
		double  & objPhi (o.shape->angle); 
		
		float  & jitterx (o.jitterx), & jittery (o.jittery), & jitterz(o.jitterz);
		
		float  & objcolor (o.color);
		
		double objLen (o.shape->scale.x * objLen_o), objLen_min (o.shape->scale.y * objLen_min_o); 

		// local target jitter
		if (jitterlocal) {
			jitterx = (ran1Gen()*jittermag - jittermag/2);
			jittery = (ran1Gen()*jittermag - jittermag/2);
			if (jitterInZ)
				jitterz = (ran1Gen()*jittermag - jittermag/2);
			else
				jitterz = 0.;
		}
		else 
			jitterx = jittery = jitterz = 0.;
		
		const int niters = ((int)fps_mode)+1; // hack :)
		
		for (int k=0; k < niters; k++) {
				if (have_fv_input_file && frameVars) {
					QVector<double> & defs = frameVars->variableDefaults();
					// re-set the defaults because we *know* what the frameNum and subframeNum *should* be!
					defs[0] = frameNum;
					defs[2] = k;
				}
				QVector<double> fv;

				Rect aabb = o.shape->AABB();

				{ // MOVEMENT/ROTATION computation happens *always* but the fvar file variables below may override the results of this computation!
						if (moveFlag) {
							
							// wrap objects that floated past the edge of the screen
							if (wrapEdge && !canvasAABB.intersects(aabb))
								wrapObject(o, aabb);
							
							
							if (!wrapEdge) {
								
								// adjust for wall bounce 
								if ((cx + cvx + aabb.size.w/2 > max_x_pix) ||  (cx + cvx - aabb.size.w/2 < min_x_pix))
									vx = -vx;
								if ((cy + cvy + aabb.size.h/2 > max_y_pix) || (cy + cvy  - aabb.size.h/2 < min_y_pix))
									vy = -vy; 
								if (z + vz < -cameraDistance || z + vz > maxZ) // hit camera or hit farthest away edge
									vz = -vz;
							
							}
							
							// initialize position iff k==0 and frameNum is a multiple of tframes
							if ( !k && !(frameNum%tframes)) {
								if (++o.vel_vec_i >= o.vel_vec.size()) {
									o.vel_vec_i = 0;
									++o.len_vec_i;
								}
								if (o.len_vec_i >= o.len_vec.size()) o.len_vec_i = 0;
								
								objLen = o.len_vec[o.len_vec_i].x;
								objLen_min = o.len_vec[o.len_vec_i].y;
								
								
								// apply new length by adjusting object scale
								o.shape->scale.x = objLen / objLen_o;
								o.shape->scale.y = objLen_min / objLen_min_o;
								
								objVelx = o.vel_vec[o.vel_vec_i].x;
								objVely = o.vel_vec[o.vel_vec_i].y;
								objVelz = o.vel_vec[o.vel_vec_i].z;
								
								// init position
								//if (0 == rndtrial) {
								// setup defaults for x,y,z and vx,vy,vz
								// these may be overridden by rndtrial code below
									x = objXinit;
									y = objYinit;
									z = objZinit;
									vx = objVelx; 
									vy = objVely;
									vz = objVelz;
								//} else 
								if (1 == rndtrial) {
									// random position and random V based on init sped 
									Vec2 cpos (ran1Gen()*canvasAABB.size.x + min_x_pix,
											   ran1Gen()*canvasAABB.size.y + min_y_pix);
									o.shape->setDistance(1.0); // TODO: have random distance too?
									o.shape->setCanvasPosition(cpos);
									vx = ran1Gen()*objVelx*2 - objVelx;
									vy = ran1Gen()*objVely*2 - objVely;
									vz = ran1Gen()*objVelz*2 - objVelz;
								} else if (2 == rndtrial) {
									// random position and direction
									// static speed
									x = ran1Gen()*canvasAABB.size.x + min_x_pix;
									y = ran1Gen()*canvasAABB.size.y + min_y_pix;
									z = 0.; // TODO: z?
									const double r = sqrt(objVelx*objVelx + objVely*objVely);
									const double theta = ran1Gen()*2.*M_PI;
									vx = r*cos(theta); 
									vy = r*sin(theta);
									vz = 0.; // TODO: determine is this ok? default to 0 velocity? what do we do about z?
								} else if (3 == rndtrial) {
									// position at t = position(t-1) + vx, vy
									// random v based on objVelx objVely range
									if (loopCt > 0) {
										x = o.lastPos.x;
										y = o.lastPos.y;
										z = o.lastPos.z;
									}
									vx = ran1Gen()*objVelx*2. - objVelx;
									vy = ran1Gen()*objVely*2. - objVely; 
									vz = ran1Gen()*objVelz*2. - objVelz; // TODO: z ok here?
								} else  {
									// position at t = position(t-1) + vx, vy
									// static speed, random direction
									if (loopCt > 0) {
										x = o.lastPos.x;
										y = o.lastPos.y;
										z = o.lastPos.z;
									}
									const double r = sqrt(objVelx*objVelx + objVely*objVely);
									const double theta = ran1Gen()*2*M_PI;
									vx = r*cos(theta); 
									vy = r*sin(theta);
									vz = 0.; // TODO: determine what to do here with Anthony..
								}
								
								d = o.shape->distance();

								objPhi = o.phi_o;
								
								cpos = o.shape->canvasPosition(); ///< position as rendered on the canvas after distance calc
								cvel = Vec2(vx/d,vy/d); ///< velocity as apparent on the canvas after distance calc
				
								aabb = o.shape->AABB();
							}
							
							// update position after delay period
							// if jitter pushes us outside of motion box then do not jitter this frame
							if ((int(frameNum)%tframes /*- delay*/) >= 0) { 
								if (!wrapEdge && ((cx + cvx + aabb.size.w/2 + jitterx/d > max_x_pix) 
												  ||  (cx + cvx - aabb.size.w/2 + jitterx/d < min_x_pix)))
									x += vx;
								else 
									x += vx + jitterx;
								if (!wrapEdge && ((cy + cvy + aabb.size.h/2 + jittery/d > max_y_pix) 
												  || (cy + cvy - aabb.size.h/2 + jittery/d < min_y_pix))) 
									y += vy;
								else 
									y += vy + jittery;
								
								if (!wrapEdge && (z + vz + jitterz > maxZ 
												  || z + vz + jitterz < -cameraDistance)) 
									z += vz;
								else 
									z += vz + jitterz;
								
								d = o.shape->distance();
								cpos = o.shape->canvasPosition(); ///< position as rendered on the canvas after distance calc
								cvel = Vec2(vx/d,vy/d); ///< velocity as apparent on the canvas after distance calc
								aabb = o.shape->AABB();
								o.lastPos = o.shape->position; // save last pos

								// also apply spin
								objPhi += o.spin;
							}
							
						}
						
				} 
			
				if (have_fv_input_file) {
					fv = frameVars->readNext();
					if ( !frameNum ) {
						fvHasPhiCol = frameVars->hasInputColumn("phi");
						fvHasZCol = frameVars->hasInputColumn("z");
						fvHasZScaledCol = frameVars->hasInputColumn("zScaled");
					}
					if (fv.size() < NUM_FRAME_VARS && frameNum) {
						// at end of file?
						Warning() << name() << "'s frame_var file ended input, stopping plugin.";
						have_fv_input_file = false;
						stop();
						return;
					} 
					if (fv.size() < NUM_FRAME_VARS || fv[0] != frameNum) {
						Error() << "Error reading frame " << frameNum << " from frameVar file! Datafile frame num differs from current frame number!  Do all the fps_mode and numObj parameters of the frameVar file match the current fps mode and numObjs?";	
						stop();
						return;
					}
					if (fv[1] != objNum) {
						Error() << "Error reading object " << objNum << " from frameVar file! Datafile object num differs from current object number!  Do all the fps_mode and numObj parameters of the frameVar file match the current fps mode and numObjs?";	
						stop();
						return;						
					}
					if (fv[2] != k) {
						Error() << "Error reading subframe " << k << " from frameVar file! Datafile subframe num differs from current subframe numer!  Do all the fps_mode and numObj parameters of the frameVar file match the current fps mode and numObjs?";	
						stop();
						return;												
					}
					

					
					bool didInitLen = false;
					const ObjType otype = ObjType(int(fv[3]));
					double r1 = fv[6], r2 = fv[7];
					if (!k && !frameNum) {
						// do some required initialization if on frame 0 for this object to make sure r1, r2 and  obj type jive
						if (otype != o.type || !eqf(r1, objLen) || !eqf(r2, objLen_min)) {
							delete o.shape;							
							o.type = otype;
							o.len_vec[0] = Vec2(r1,r2);
							didInitLen = true;
							initObj(o);
						}		
					}
					
					x = fv[4];
					y = fv[5];
					// handle type change mid-plugin-run
					if (o.type != otype) {
						Shapes::Shape *oldshape = o.shape;
						o.shape = 0;
						o.type = otype;
						initObj(o);
						o.shape->position = oldshape->position;
						o.shape->scale = oldshape->scale;
						o.shape->color = oldshape->color;
						o.shape->angle = oldshape->angle;
						delete oldshape;
					}					
					// handle length changes mid-plugin-run
					if (!didInitLen && (!eqf(r1, objLen) || !eqf(r2, objLen))) {
						o.shape->setRadii(r1, r2);
					}
					bool zChanged = false;
					if (fvHasPhiCol) 
						objPhi = fv[8];
					if (fvHasZCol)
						z = fv[10], zChanged = true;
					else 
						z = 0.0, zChanged = false;
					
					double newZ = z;
					if (fvHasZScaledCol && fvHasZCol) {
						newZ = distanceToZ(d=fv[11]);
						if (fabs(newZ - z) >= 0.0001 && didScaledZWarning <= 3) {
							Warning() << "Encountered z & zScaled column in framevar file and they disagree.  Ignoring zScaled and using z. (Could the render window size have changed?)";						
							++didScaledZWarning;
						}
					}
					
					if (fvHasZScaledCol && !fvHasZCol) {
						z = newZ, zChanged = true;
					}
					if (zChanged) aabb = o.shape->AABB(), d = o.shape->distance();
					objcolor = fv[9];
					
				} //  end !have_fv_input_file

				// draw stim if out of delay period
				if (fv.size() || (int(frameNum)%tframes /*- delay*/) >= 0) {
					float r,g,b;

					if (fps_mode == FPS_Triple || fps_mode == FPS_Dual) {
						b = g = r = bgcolor;					
						// order of frames comes from config parameter 'color_order' but defaults to RGB
						if (k == b_index) b = objcolor;
						else if (k == r_index) r = objcolor;
						else if (k == g_index) g = objcolor;

					} else 
						b = g = r = objcolor;

					o.shape->color = Vec3(r, g, b);
					if (d > 0.0)
						o.shape->draw();
					
					///DEBUG HACK FOR AABB VERIFICATION					
					if (debugAABB && k==0) {
						Rect r = aabb;
						glColor3f(0.,1.,0); 
						glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
						glBegin(GL_QUADS);
						glVertex2f(r.origin.x,r.origin.y);
						glVertex2f(r.origin.x+r.size.w,r.origin.y);
						glVertex2f(r.origin.x+r.size.w,r.origin.y+r.size.h);
						glVertex2f(r.origin.x,r.origin.y+r.size.h);	
						glEnd();
						glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
						//objPhi += 1.;
					}					
					
					if (!fv.size()) {
						double fnum = frameNum;

						// in rndtrial mode, save 1 big file with fnum being a derived value.  HACK!
						if (rndtrial && loopCt && nFrames) fnum = frameNum + loopCt*nFrames;

						// nb: push() needs to take all doubles as args!
						frameVars->push(fnum, double(objNum), double(k), double(o.type), double(x), double(y), double(objLen), double(objLen_min), double(objPhi), double(objcolor), double(z), double(d));
					}
				}

		}
	}
}

void MovingObjects::wrapObject(ObjData & o, Rect & aabb) const {
	Shapes::Shape & s = *o.shape;

	Vec2 cpos = s.canvasPosition();
	
	// wrap right edge
	if (aabb.left() >= canvasAABB.right()) cpos.x = (canvasAABB.left()-aabb.size.w/2) + (aabb.left() - canvasAABB.right());
	// wrap left edge
	if (aabb.right() <= canvasAABB.left()) cpos.x = (canvasAABB.right()+aabb.size.w/2) - (canvasAABB.left()-aabb.right());
	// wrap top edge
	if (aabb.bottom() >= canvasAABB.top()) cpos.y = (canvasAABB.bottom()-aabb.size.h/2) + (aabb.bottom() - canvasAABB.top());
	// wrap bottom edge
	if (aabb.top() <= canvasAABB.bottom()) cpos.y = (canvasAABB.top()+aabb.size.h/2) - (canvasAABB.bottom()-aabb.top());
	
	s.setCanvasPosition(cpos);
	aabb = o.shape->AABB();
}

void MovingObjects::initCameraDistance()
{
	majorPixelWidth = width() > height() ? width() : height();
	const double dummyObjLen = 25.0;
	double bestDiff = 1e9, bestDist = 1e9;
	// d=15; dd=101; (rad2deg(2 * atan(.5 * (d/dd))) / 90) * 640
	for (double D = 0.; D < majorPixelWidth; D += 1.0) {
		double diff = fabs(dummyObjLen - (RAD2DEG(2. * atan(.5 * (dummyObjLen/D))) / 90.) * majorPixelWidth);
		if (diff < bestDiff) bestDiff = diff, bestDist = D;
	}
	cameraDistance = bestDist;
	Log() << "Camera distance in Z is " << cameraDistance;
}

double MovingObjects::distanceToZ(double d) const
{
	return (cameraDistance * d) - cameraDistance;
}

double MovingObjects::zToDistance(double z) const
{
	if (cameraDistance <= 0.) return 0.;
	return z/cameraDistance + 1.0;
}
