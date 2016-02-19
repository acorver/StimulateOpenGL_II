#ifndef CheckerFlicker_H
#define CheckerFlicker_H
#include "StimPlugin.h"
#include <deque>
#include <vector>
#include <QReadWriteLock>
#include "Util.h"

class GLWindow;
struct Frame;
class FrameCreator;

enum Rand_Gen {
	Uniform = 0, Gauss, Binary, N_Rand_Gen
};

struct IntArray8 {
	GLint i[8];
};

/** A struct that captures the frametrack state.  This is to implement a delayed ftrack box change
 that goes with each generated frame. Used in CheckerFlicker class. */
struct FTrack_Params {	 
	Vec3i xyw;///< frame track box x, y, and width.. saved StimPlugin::ftrackbox_x,y,w
	Vec3 colors[StimPlugin::N_FTStates]; ///< saved StimPlugin::ftStateColors[]
};

/** \brief An internal struct that encapsulates a frame generated by a FrameCreator thread.
 
 Frames are generated by the FrameCreator and queued, and then dequeued in
 side the mainthread in CheckerFlicker::afterVSync().*/
struct Frame
{
	Frame();
    Frame(unsigned w, unsigned h, unsigned elem_size);
    ~Frame() { cleanup(); }
	void cleanup() { if (mem) { delete [] mem; mem = 0; texels = 0; } }
	void init();
	void copyProperties(const Frame *);
	
	int param_serial; ///< which params from the history were used to generate this frame
    GLubyte *mem; ///< this is the memory block that is an unaligned superset of texels and should the the one we delete []
    GLvoid *texels; ///< 16-bytes aligned texel area (useful for SFMT-sse2 rng)
	unsigned tx_size; ///< size of each texel in bytes
    unsigned long nqqw; ///< num of quad-quad words.. (128-bit words)
	
    // presently not used..
    int ifmt, fmt, type; ///< for gltexsubimage..
	
    unsigned w, h; ///< in texels
	
	int width_pix, height_pix;
	int lmargin, rmargin, bmargin, tmargin;
	
	Vec2i displacement; ///< frame displacement in pixels in the x and y direction
	float bgcolor; ///< saved clearcolor	
	
	IntArray8 texCoords, vertices; ///< Used to display the texture.. computed values based on extant params at time of frame generation
	
	FTrack_Params ftrack_params; ///< the exact frame track params that were used to generate this frame
	
	void setupTexCoords();
};


// SFMT based random number generator	
#include "sfmt.hpp"
typedef sfmt_19937_generator SFMT_Generator;

/**
   \brief A class for drawing randomly-generated 'checkers' of arbitrary width 
          and height to the GLWindow.  


   For a full description of this plugin's parameters, it is recommended you 
   see the \subpage plugin_params "Plugin Parameter Documentation"  for more 
   details.
*/
class CheckerFlicker : public StimPlugin
{
	Q_OBJECT

    friend class FrameCreator;

	SFMT_Generator sfmt;
	
    std::vector<FrameCreator *> fcs;
    void cleanupFCs();

    int stixelWidth;	///< width of stixel in x direction
    int stixelHeight;	///< height of stixel in y direction
	Rand_Gen rand_gen;
    float meanintensity; ///< mean light intensity between 0 and 1
    float contrast;	 ///< defined as Michelsen contrast for black/white mode
    // and as std/mean for gaussian mode
    int w, h;           ///< window width/height cached here
    int originalSeed, currentSFMTSeed;	///< seed for random number generator at initialization
	QReadWriteLock sharedParamsRWLock; ///< used to protect Nx, Ny, and other that framecreator thread *reads* and main thread may write to on realtime param update
    int Nx;		///< number of stixels in x direction
    int Ny;		///< number of stixels in y direction
    int xpixels, ypixels;
    unsigned nCoresMax;
    unsigned fbo;       ///< iff nonzero, the number of framebuffer objects to use for prerendering -- this is faster than `prerender' but not always supported?
    int ifmt, fmt, type; ///< for glTexImage2D() call..
    int nConsecSkips;
	bool verboseDebug; ///< iff true, spam lots of debug output to app console 
	unsigned rand_displacement_x, rand_displacement_y; ///< if either are nonzero, each frame is displaced by this amont in pixels (not stixels!) randomly in x or y direction
    // Note only one of display_lists, prerender, or fbo may be active above
	
	
	/// Saved ftrackbox_x, ftrackbox_y, and ftrackbox_w, to make it so that realtime param updates don't change 
	/// the ftrackbox immediately.  instead, these params get saved with the Frame
	FTrack_Params ftrack_params;
	/// Called from drawFrame to use a Frame's saved ftrack_params in the real StimPlugin::ftrackbox_* params..
	void useFTrackParams(const FTrack_Params & p);

	
    GLuint *fbos, *texs; ///< array of fbo object id's and the texture ids iff fbo is nonzero
	
	std::vector<Frame> frames; ///< indexed by getNum/putNum mechanism -- all ->mem and ->tex members are NULL here -- just contains display param data that was used to generate each frame!
	
    friend class GLWindow;
    unsigned gaussColorMask; ///< this will always be a power of 2 minus 1
    std::vector<GLubyte> gaussColors;
	std::vector<float> gaussColorsUnscaled;
	float origGaussContrast, origGaussBGColor;
    void genGaussColors();
    inline GLubyte getColor(unsigned entropy) { return gaussColors[entropy&gaussColorMask]; }
	inline GLubyte getColorR(unsigned entropy, float bgc, float cont) {
		const float c_unsc = gaussColorsUnscaled[entropy&gaussColorMask];
		// this means contrast or bgcolor changed as a realtime param update, so scale it in realtime and don't used cached value... 		
		return static_cast<GLubyte>( ((c_unsc*(bgc*cont)) + bgc) * 256.0f );
	}

    std::deque<unsigned> nums, oldnums; ///< queue of texture indices into the texs[] array above.  oldest onest are in back, newest onest in front
    unsigned num;
    double lastAvgTexSubImgProcTime, minTexSubImageProcTime, maxTexSubImageProcTime;
    volatile int lastFramegen;
	volatile unsigned frameGenAvg_usec;
	unsigned origThreadAffinityMask;
	double t0reapply;

    inline void putNum() { oldnums.push_back(num);  }
    inline unsigned takeNum() { 
        if (nums.size()) {
            num = nums.front(); 
            nums.pop_front(); 
            return num;
        } else if (oldnums.size()) {
			// if we got here it means we are skipping frames as fc thread is not fast enough
            num = oldnums.front(); 
            oldnums.pop_front();
            return num; 
        } // else...
        Error() << "INTERNAL ERROR: takeNum() ran out of numbers!";
        return 0;
    }
    inline unsigned newFrameNum()  { 
        unsigned num;
        if (oldnums.size()) {
            num = oldnums.front();
            oldnums.pop_front();
            nums.push_back(num);
            return num;
        } // else...
        Error() << "INTERNAL ERROR: newFrameNum() ran out of numbers in oldnums!";
        return 0;
    }
    void setNums();

    Frame *genFrame(std::vector<unsigned> & entropy_buf, SFMT_Generator & sfmt_generator_to_use);

    bool initPrerender();  ///< init for 'prerender to sysram'
    bool initFBO(); ///< init for 'prerender to FBO'
    void cleanupPrerender(); ///< cleanup for prerender frames mode
    void cleanupFBO(); ///< cleanup for FBO mode
	Rand_Gen parseRandGen(const QString &) const;
	bool initFromParams(bool runtimeReapply = false); ///< reusable init code for both real init and realtime apply params init
	bool checkForCriticalParamChanges(); ///< called by applyNewParamsAtRuntime() to see if we need to do a full-reinit
	void doPostInit(); ///< called by init() after full re-init
	
	int param_serial;
	bool paramHistoryPushPendingFlag, needToUnlockRWLock;
	
protected:
    CheckerFlicker(); ///< can only be constructed by our friend class
    ~CheckerFlicker();

    /// Draws the next frame that is already in the texture buffer to the screen.
    void drawFrame();
    /// Informs FrameCreator threads to generate more frames and pops off 1 Frame from a FrameCreator queue and loads it onto the video board using FBO
    void afterVSync(bool isSimulated = false);
    bool init();
	unsigned initDelay(void); ///< reimplemented from superclass -- returns an init delay of 500ms
    void cleanup(); 
    void save();
	/* virtual */ bool applyNewParamsAtRuntime(); ///< reimplemented from superclass -- reapplies new params at runtime and reinits state/frame creators if need be
	/* virtual */ bool applyNewParamsAtRuntime_Base(); ///< reimplemented from superclass
	/// Called by GLWindow.cpp when new parameters are accepted.  Overrides StimPlugin parent and sets a flag.  The actual param history is pushed once the new frame hits the screen!
	virtual void newParamsAccepted();
	/// Reimplemented from StimPlugin for our custom param history pending checks
    virtual void checkPendingParamHistory(bool *isAODOOnlyChanges = 0, ChangedParamMap *aodoOnlyParams = 0);
};


#endif
