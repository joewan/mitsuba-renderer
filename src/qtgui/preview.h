/*
    This file is part of Mitsuba, a physically based rendering system.

    Copyright (c) 2007-2011 by Wenzel Jakob and others.

    Mitsuba is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License Version 3
    as published by the Free Software Foundation.

    Mitsuba is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#if !defined(__PREVIEW_H)
#define __PREVIEW_H

#include <mitsuba/core/platform.h>
#include <QtGui>
#include <mitsuba/hw/vpl.h>
#include <mitsuba/hw/direct.h>
#include <mitsuba/render/preview.h>
#include "common.h"
#include "preview_proc.h"
#include "mitsuba/hw/framebufferobject.h"

using namespace mitsuba;

struct SceneContext;

//#define SSSDEBUG

/**
 * Asynchronous preview rendering thread
 */
class PreviewThread : public QObject, public Thread {
	Q_OBJECT
public:
    /* realime sss related structures */
    typedef struct {
        /* Splat origin */
        Vector3 o;
        /* Incident color */
        Vector3 c;
    } Splat;

    typedef struct {
        const TriMesh* mesh;
        ref<GPUTexture> albedoMap;
        ref<GPUTexture> diffusionMap;
        Float splatRadius;
    } TranslucentShape;

public:
	PreviewThread(Device *parentDevice, Renderer *parentRenderer);

	/**
	 * Change the scene context. 
	 */
	void setSceneContext(SceneContext *context, bool swapContext, bool motion);

	/**
	 * Resume rendering 
	 */
	void resume();

	/**
	 * Wait until the thread has started
	 */
	inline void waitUntilStarted() { m_started->wait(); }

	/**
	 * Acquire the best current approximation of the rendered scene.
	 * If that takes longer than 'ms' milliseconds, a failure occurs
	 */
	PreviewQueueEntry acquireBuffer(int ms = -1);

	/// Return the buffer to the renderer
	void releaseBuffer(PreviewQueueEntry &entry);
	
	/// Terminate the preview thread
	void quit();
signals:
	void caughtException(const QString &what);
	void statusMessage(const QString &status);
protected:
	/// Preview thread main loop
	virtual void run();
	/// Virtual destructure
	virtual ~PreviewThread();
	/// Render a single VPL using OpenGL
	void oglRenderVPL(PreviewQueueEntry &target, const VPL &vpl);
    /// Render face or vertex normals of all meshes
    void oglRenderNormals(const std::vector<std::pair<const TriMesh *, Transform> > meshes);
	/// Render a single VPL using real-time coherent ray tracing
	void rtrtRenderVPL(PreviewQueueEntry &target, const VPL &vpl);

    /* realtime sss */

    /// Render a multi-pass OpenGL visalisation that supports realtime SSS
    void oglRender(PreviewQueueEntry &target);
    /// check for OpenGL errors
    void oglErrorCheck();
    /// calculate splat positon and colors in light view
    void calcSplatPositions(const TranslucentShape &ts, Point &camPos);
    /// calculate in view space
    void calcVisiblePositions(const TranslucentShape &ts);
    /// build the splats und accumulate them in view space
    void combineSplats(const TranslucentShape &ts);

private:
	ref<Session> m_session;
	ref<Device> m_device, m_parentDevice;
	ref<Renderer> m_renderer, m_parentRenderer;
	ref<VPLShaderManager> m_shaderManager;
	ref<DirectShaderManager> m_directShaderManager;
	ref<GPUTexture> m_framebuffer;
	ref<GPUProgram> m_accumProgram;
	ref<PreviewProcess> m_previewProc;
	ref<Random> m_random;
	int m_accumProgramParam_source1;
	int m_accumProgramParam_source2;
	const GPUTexture *m_accumBuffer;
	ref<Mutex> m_mutex;
	ref<ConditionVariable> m_queueCV;
	ref<Timer> m_timer;
	ref<WaitFlag> m_started;
	std::list<PreviewQueueEntry> m_readyQueue, m_recycleQueue;
	SceneContext *m_context;
	size_t m_vplSampleOffset;
	int m_minVPLs, m_vplCount;
	int m_vplsPerSecond, m_raysPerSecond;
	int m_bufferCount, m_queueEntryIndex;
	std::deque<VPL> m_vpls;
	std::vector<GPUTexture *> m_releaseList;
	Point m_camPos;
	Transform m_camViewTransform;
	Float m_backgroundScaleFactor;
	bool m_quit, m_sleep, m_motion, m_useSync;
	bool m_refreshScene;

    /* realtime sss related */

    ref<FrameBufferObject> fboLightView;
    ref<FrameBufferObject> fboView;
    ref<FrameBufferObject> fboViewExpand;
    ref<FrameBufferObject> fboCumulSplat;
    ref<FrameBufferObject> fboTmp;
    /* One diffusion map per mesh would be needed for multiple SSS configurations
     * in one scene. For now, support only one. */
    ref<GPUTexture> diffusionMap;
    ref<GPUTexture> albedoMap;

    SpotLight m_currentSpot;
    std::vector<Splat> splats;

    // the number of overlapping splats
    int n0;
    //light view subsurface data buffer size (square for a spot with constant aperture)
    int fboSplatSize;
    float *splatOrigins;
    float *splatColors;

    //cumulative splat buffer resolution to accelerate the rendering
    int fboCumulSplatWidth;
    int fboCumulSplatHeight;

    static unsigned int intColFormRGBF[2];
    static unsigned int intColFormRGBAF[2];
    static unsigned int intColFormRGBAF32[2];
    static unsigned int filter[2];
    static unsigned int filterL[2];
    static unsigned int wrap[2];
    static unsigned int wrapE[2];

#ifdef SSSDEBUG
    std::vector<const TriMesh*> m_exportedMeshes;
    std::vector<const TriMesh*> m_camViewImages;
    std::vector<const TriMesh*> m_expansionImages;
    std::vector<const TriMesh*> m_finalImages;
#endif
};

#endif /* __PREVIEW_H */
