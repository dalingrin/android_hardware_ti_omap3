/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef ANDROID_HARDWARE_CAMERA_HARDWARE_H
#define ANDROID_HARDWARE_CAMERA_HARDWARE_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <utils/Log.h>
#include <utils/threads.h>
#include <linux/videodev2.h>
#include "binder/MemoryBase.h"
#include "binder/MemoryHeapBase.h"
#include <utils/threads.h>
#include <ui/CameraHardwareInterface.h>
#include "MessageQueue.h"
#include "overlay_common.h"
#include "Semaphore.h"
#include "CameraProperties.h"

#define MIN_WIDTH           128
#define MIN_HEIGHT          96
#define PICTURE_WIDTH   3296 /* 5mp - 2560. 8mp - 3280 */ /* Make sure it is a multiple of 16. */
#define PICTURE_HEIGHT  2464 /* 5mp - 2048. 8mp - 2464 */ /* Make sure it is a multiple of 16. */
#define PREVIEW_WIDTH 176
#define PREVIEW_HEIGHT 144
#define PIXEL_FORMAT           V4L2_PIX_FMT_UYVY

#define VIDEO_FRAME_COUNT_MAX    NUM_OVERLAY_BUFFERS_REQUESTED
#define MAX_CAMERA_BUFFERS    NUM_OVERLAY_BUFFERS_REQUESTED
#define MAX_ZOOM        3
#define THUMB_WIDTH     80
#define THUMB_HEIGHT    60
#define PIX_YUV422I 0
#define PIX_YUV420P 1

#define DEBUG_LOG 1
///Camera HAL Logging Functions
#ifndef DEBUG_LOG

#define CAMHAL_LOGDA(str)
#define CAMHAL_LOGDB(str, ...)
#define LOG_FUNCTION_NAME
#define LOG_FUNCTION_NAME_EXIT

#else

#define CAMHAL_LOGDA(str) LOGD("%s:%d %s - " str,__FILE__, __LINE__,__FUNCTION__);
#define CAMHAL_LOGDB(str, ...) LOGD("%s:%d %s - " str,__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__);
#define LOG_FUNCTION_NAME    LOGE("%d: %s() ENTER", __LINE__, __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT    LOGE("%d: %s() EXIT", __LINE__, __FUNCTION__);

#endif

#define CAMHAL_LOGEA(str) LOGE("%s:%d %s - " str,__FILE__, __LINE__, __FUNCTION__);
#define CAMHAL_LOGEB(str, ...) LOGE("%s:%d %s - " str,__FILE__, __LINE__,__FUNCTION__, __VA_ARGS__);



#define NONNEG_ASSIGN(x,y) \
    if(x > -1) \
        y = x

namespace android {


/** TODO: Check if we can include this structure from v4l2_utlils.h or define it in some TI top level header file**/
/*
 * This is the overlay_t object, it is returned to the user and represents
 * an overlay. here we use a subclass, where we can store our own state.
 * This handles will be passed across processes and possibly given to other
 * HAL modules (for instance video decode modules).
 */
struct overlay_true_handle_t : public native_handle {
    /* add the data fields we need here, for instance: */
    int ctl_fd;
    int shared_fd;
    int width;
    int height;
    int format;
    int num_buffers;
    int shared_size;
};

/* Defined in liboverlay */
typedef struct {
    int fd;
    size_t length;
    uint32_t offset;
    void *ptr;
} mapping_data_t;


#define PHOTO_PATH          "/tmp/photo_%02d.%s"

#define PROC_THREAD_PROCESS     0x5
#define PROC_THREAD_EXIT        0x6
#define PROC_THREAD_NUM_ARGS    26
#define SHUTTER_THREAD_CALL     0x1
#define SHUTTER_THREAD_EXIT     0x2
#define SHUTTER_THREAD_NUM_ARGS 3
#define RAW_THREAD_CALL         0x1
#define RAW_THREAD_EXIT         0x2
#define RAW_THREAD_NUM_ARGS     4
#define SNAPSHOT_THREAD_START   0x1
#define SNAPSHOT_THREAD_EXIT    0x2

#define PAGE                    0x1000
#define PARAM_BUFFER            512

///Forward declarations
class CameraHal;
class CameraFrame;
class CameraHalEvent;
class DisplayFrame;

///@todo See if we can club these type declarations into a namespace
///      Have a generic callback class based on template - to adapt CameraFrame and Event
typedef void (*frame_callback)(CameraFrame *cameraFrame);
typedef void (*event_callback) (CameraHalEvent *event);

/**
  * Interface class implemented by classes that have some events to communicate to dependendent classes
  * Dependent classes use this interface for registering for events
  */
class MessageNotifier
{
public:
    static const uint32_t EVENT_BIT_FIELD_POSITION;
    static const uint32_t FRAME_BIT_FIELD_POSITION;

    ///@remarks Msg type comes from CameraFrame and CameraHalEvent classes
    ///           MSB 16 bits is for events and LSB 16 bits is for frame notifications
    ///         FrameProvider and EventProvider classes act as helpers to event/frame
    ///         consumers to call this api
    virtual void enableMsgType(int32_t msgs, frame_callback frameCb=NULL, event_callback eventCb=NULL, void* cookie=NULL) = 0;
    virtual void disableMsgType(int32_t msgs, void* cookie) = 0;

    virtual ~MessageNotifier() {};
};

class ErrorNotifier
{
public:
    virtual void errorNotify(int error) = 0;

    virtual ~ErrorNotifier() {};
};


/**
  * Interace class abstraction for Camera Adapter to act as a frame provider
  * This interface is fully implemented by Camera Adapter
  */
class FrameNotifier : public MessageNotifier
{
public:
    virtual void returnFrame(void* frameBuf) = 0;

    virtual ~FrameNotifier() {};
};

/**   * Wrapper class around Frame Notifier, which is used by display and notification classes for interacting with Camera Adapter
  */
class FrameProvider
{
    FrameNotifier* mFrameNotifier;
    void* mCookie;
    frame_callback mFrameCallback;

public:
    FrameProvider(FrameNotifier *fn, void* cookie, frame_callback frameCallback)
        :mFrameNotifier(fn), mCookie(cookie),mFrameCallback(frameCallback) { }

    int enableFrameNotification(int32_t frameTypes);
    int disableFrameNotification(int32_t frameTypes);
    int returnFrame(void *frameBuf);
};

/** Wrapper class around MessageNotifier, which is used by display and notification classes for interacting with
   *  Camera Adapter
  */
class EventProvider
{
public:
    MessageNotifier* mEventNotifier;
    void* mCookie;
    event_callback mEventCallback;

public:
    EventProvider(MessageNotifier *mn, void* cookie, event_callback eventCallback)
        :mEventNotifier(mn), mCookie(cookie), mEventCallback(eventCallback) {}

    int enableEventNotification(int32_t eventTypes);
    int disableEventNotification(int32_t eventTypes);
};

/*
  * Interface for providing buffers
  */
class BufferProvider
{
public:
    virtual void* allocateBuffer(int width, int height, const char* format, int bytes, int numBufs) = 0;
    virtual int freeBuffer(void* buf) = 0;

    virtual ~BufferProvider() {}
};

struct CameraFrame
    {
    enum FrameType
    {
        PREVIEW_FRAME_SYNC=0x1, ///SYNC implies that the frame needs to be explicitly returned after consuming in order to be filled by camera again
        PREVIEW_FRAME=0x2   , ///Preview frame includes viewfinder and snapshot frames
        IMAGE_FRAME_SYNC=0x4, ///Image Frame is the image capture output frame
        IMAGE_FRAME=0x8,
        VIDEO_FRAME_SYNC=0x10, ///Timestamp will be updated for these frames
        VIDEO_FRAME=0x20,
        FRAME_DATA_SYNC=0x40, ///Any extra data assosicated with the frame. Always synced with the frame
        FRAME_DATA=0x80,
        ALL_FRAMES=0xFFFF   ///Maximum of 16 frame types supported
    };

    void *mCookie;
    void *mBuffer;

    unsigned int mTimestamp;
    ///@todo add other member vars like offset, stride, width, height, etc
    };

///Common Camera Hal Event class which is visible to CameraAdapter, DisplayAdapter and AppCallbackNotifier
///This class describes the event in the callback from CameraAdapter to DisplayAdapter and AppCallbackNotifier
///@todo Rename this class to CameraEvent
class CameraHalEvent
{
public:
    //Enums
    enum CamerHalEventType
        {
        EVENT_FOCUS_LOCKED = 0x1,
        EVENT_FOCUS_ERROR = 0x2,
        EVENT_ZOOM_LEVEL_REACHED = 0x4,
        ///@remarks Future enum related to display, like frame displayed event, could be added here
        ALL_EVENTS = 0xFFFF ///Maximum of 16 event types supported
        };

    typedef void* CameraHalEventData;

    ///Class declarations
    ///@remarks Add a new class for a new event type added above

    ///Focus event specific data
    class FocusEventData
        {
        public:
            bool focusLocked;
            bool focusError;
            int currentFocusValue;
        };

    ///Zoom specific event data
    class ZoomEventData
        {
        public:
            int currentZoomValue;
            bool targetZoomLevelReached;
        };

    void* mCookie;
    CamerHalEventType mEventType;
    CameraHalEventData *mEventData;

};


/**
  * Class for handling data and notify callbacks to application
  */
class   AppCallbackNotifier: public ErrorNotifier, public virtual RefBase
{

public:

    ///Constants
    static const int NOTIFIER_TIMEOUT;

    enum NotifierCommands
        {
        NOTIFIER_CMD_PROCESS_EVENT,
        NOTIFIER_CMD_PROCESS_FRAME,
        NOTIFIER_CMD_PROCESS_ERROR
        };

    enum NotifierState
        {
        NOTIFIER_STOPPED,
        NOTIFIER_STARTED,
        NOTIFIER_EXITED
        };

public:

    ~AppCallbackNotifier();

    ///Initialzes the callback notifier, creates any resources required
    status_t initialize();

    ///Starts the callbacks to application
    status_t start();

    ///Stops the callbacks from going to application
    status_t stop();

    void setEventProvider(int32_t eventMask, MessageNotifier * eventProvider);
    void setFrameProvider(FrameNotifier *frameProvider);

    //All sub-components of Camera HAL call this whenever any error happens
    virtual void errorNotify(int error);


    //thread loops
    void notificationThread();

    ///Notification callback functions
    static void frameCallbackRelay(CameraFrame* caFrame);
    static void eventCallbackRelay(CameraHalEvent* chEvt);
    void frameCallback(CameraFrame* caFrame);
    void eventCallback(CameraHalEvent* chEvt);

    //Internal class definitions
    class NotificationThread : public Thread {
        AppCallbackNotifier* mAppCallbackNotifier;
        MessageQueue mNotificationThreadQ;
    public:
        enum NotificationThreadCommands
        {
        NOTIFIER_START,
        NOTIFIER_STOP,
        NOTIFIER_EXIT,
        };
    public:
        NotificationThread(AppCallbackNotifier* nh)
            : Thread(false), mAppCallbackNotifier(nh) { }
        virtual bool threadLoop() {
            mAppCallbackNotifier->notificationThread();
            return false;
        }

        MessageQueue &msgQ() { return mNotificationThreadQ;}
    };

    //Friend declarations
    friend class NotificationThread;

private:
    void notifyEvent();
    void notifyFrame();
    bool processMessage();

private:
    CameraHal *mHardware;
    sp< NotificationThread> mNotificationThread;
    EventProvider *mEventProvider;
    FrameProvider *mFrameProvider;
    MessageQueue mEventQ;
    MessageQueue mFrameQ;
    NotifierState mNotifierState;

};


/**
  * Class used for allocating memory for JPEG bit stream buffers, output buffers of camera in no overlay case
  */
class MemoryManager : public BufferProvider, public virtual RefBase
{
public:
    ///Initializes the display adapter creates any resources required
    status_t initialize(){ return NO_ERROR; }

    virtual void* allocateBuffer(int width, int height, const char* format, int bytes, int numBufs);
    virtual int freeBuffer(void* buf);
};




/**
  * CameraAdapter interface class
  * Concrete classes derive from this class and provide implementations based on the specific camera h/w interface
  */

class CameraAdapter: public FrameNotifier, public virtual RefBase
{
public:

    enum CameraCommands
        {
        CAMERA_START_PREVIEW,
        CAMERA_STOP_PREVIEW,
        CAMERA_START_IMAGE_CAPTURE,
        CAMERA_STOP_IMAGE_CAPTURE,
        CAMERA_PERFORM_AUTOFOCUS,
        CAMERA_PREVIEW_FLUSH_BUFFERS,
        CAMERA_USE_BUFFERS
        };

    enum CameraMode
        {
        CAMERA_PREVIEW,
        CAMERA_IMAGE_CAPTURE,
        CAMERA_VIDEO,
        };
public:

    ///Initialzes the camera adapter creates any resources required
    virtual status_t initialize() = 0;

    ///@todo Change the signature to return status_t or void
    virtual int setErrorHandler(ErrorNotifier *errorNotifier) = 0;

    //Message/Frame notification APIs
    virtual void enableMsgType(int32_t msgs, frame_callback callback=NULL, event_callback eventCb=NULL, void* cookie=NULL) = 0;
    virtual void disableMsgType(int32_t msgs, void* cookie) = 0;
    virtual void returnFrame(void* frameBuf) = 0;

    //APIs to configure Camera adapter and get the current parameter set
    virtual status_t setParameters(const CameraParameters& params) = 0;
    virtual CameraParameters getParameters() const = 0;

    //API to get the caps
    virtual status_t getCaps() = 0;

    //API to give the buffers to Adapter
    status_t useBuffers(CameraMode mode, void* bufArr, int num)
        {
        return sendCommand(CameraAdapter::CAMERA_USE_BUFFERS, (int)mode, (int)bufArr, (int)num);
        }

    //API to flush the buffers from Camera
     status_t flushBuffers()
        {
        return sendCommand(CameraAdapter::CAMERA_PREVIEW_FLUSH_BUFFERS);
        }

    //API to send a command to the camera
    virtual status_t sendCommand(int operation, int value1=0, int value2=0, int value3=0) = 0;

    //API to cancel a currently executing command
    virtual status_t cancelCommand(int operation) = 0;

    //API to get the frame size required to be allocated. This size is used to override the size passed
    //by camera service when VSTAB/VNF is turned ON for example
    virtual void getFrameSize(int &width, int &height) = 0;


    virtual ~CameraAdapter() {};
};


class DisplayAdapter : public BufferProvider, public virtual RefBase
{
public:

    ///Initializes the display adapter creates any resources required
    virtual status_t initialize() = 0;

    virtual int setOverlay(const sp<Overlay> &overlay) = 0;
    virtual int setFrameProvider(FrameNotifier *frameProvider) = 0;
    virtual int setErrorHandler(ErrorNotifier *errorNotifier) = 0;
    virtual int enableDisplay() = 0;
    virtual int disableDisplay() = 0;
        virtual int useBuffers(void *bufArr, int num) = 0;
    virtual bool supportsExternalBuffering() = 0;

};


 /**
    Implementation of the Android Camera hardware abstraction layer

    This class implements the interface methods defined in CameraHardwareInterface
    for the OMAP4 platform

    @note This class has undergone major re-architecturing from OMAP3

*/
class CameraHal : public CameraHardwareInterface {


public:
    ///Constants
    static const int NO_BUFFERS_PREVIEW;
    static const int NO_BUFFERS_IMAGE_CAPTURE;



    /*--------------------Interface Methods---------------------------------*/

    /** @name externalFunctions
     * Functions defined in CameraHardwareInterface class
     */
     //@{
public:
        /** Return the IMemoryHeap for the preview image heap */
        virtual sp<IMemoryHeap>         getPreviewHeap() const;

        /** Return the IMemoryHeap for the raw image heap */
        virtual sp<IMemoryHeap>         getRawHeap() const;

        /** Set the notification and data callbacks */
        virtual void setCallbacks(notify_callback notify_cb,
                                  data_callback data_cb,
                                  data_callback_timestamp data_cb_timestamp,
                                  void* user);

        /**
         * The following three functions all take a msgtype,
         * which is a bitmask of the messages defined in
         * include/ui/Camera.h
         */

        /**
         * Enable a message, or set of messages.
         */
        virtual void        enableMsgType(int32_t msgType);

        /**
         * Disable a message, or a set of messages.
         */
        virtual void        disableMsgType(int32_t msgType);

        /**
         * Query whether a message, or a set of messages, is enabled.
         * Note that this is operates as an AND, if any of the messages
         * queried are off, this will return false.
         */
        virtual bool        msgTypeEnabled(int32_t msgType);

        /**
         * Start preview mode.
         */
        virtual status_t    startPreview();

        /**
         * Only used if overlays are used for camera preview.
         */
        virtual bool         useOverlay(){return true;}
        virtual status_t     setOverlay(const sp<Overlay> &overlay);

        /**
         * Stop a previously started preview.
         */
        virtual void        stopPreview();

        /**
         * Returns true if preview is enabled.
         */
        virtual bool        previewEnabled();

        /**
         * Start record mode. When a record image is available a CAMERA_MSG_VIDEO_FRAME
         * message is sent with the corresponding frame. Every record frame must be released
         * by calling releaseRecordingFrame().
         */
        virtual status_t    startRecording();

        /**
         * Stop a previously started recording.
         */
        virtual void        stopRecording();

        /**
         * Returns true if recording is enabled.
         */
        virtual bool        recordingEnabled();

        /**
         * Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
         */
        virtual void        releaseRecordingFrame(const sp<IMemory>& mem);

        /**
         * Start auto focus, the notification callback routine is called
         * with CAMERA_MSG_FOCUS once when focusing is complete. autoFocus()
         * will be called again if another auto focus is needed.
         */
        virtual status_t    autoFocus();

        /**
         * Cancels auto-focus function. If the auto-focus is still in progress,
         * this function will cancel it. Whether the auto-focus is in progress
         * or not, this function will return the focus position to the default.
         * If the camera does not support auto-focus, this is a no-op.
         */
        virtual status_t    cancelAutoFocus();

        /**
         * Take a picture.
         */
        virtual status_t    takePicture();

        /**
         * Cancel a picture that was started with takePicture.  Calling this
         * method when no picture is being taken is a no-op.
         */
        virtual status_t    cancelPicture();

        /** Set the camera parameters. */
        virtual status_t    setParameters(const CameraParameters& params);

        /** Return the camera parameters. */
        virtual CameraParameters  getParameters() const;

        /**
         * Send command to camera driver.
         */
        virtual status_t sendCommand(int32_t cmd, int32_t arg1, int32_t arg2);

        /**
         * Release the hardware resources owned by this object.  Note that this is
         * *not* done in the destructor.
         */
        virtual void release();

        /**
         * Dump state of the camera hardware
         */
        virtual status_t dump(int fd, const Vector<String16>& args) const;

     //@}

/*--------------------Internal Member functions - Public---------------------------------*/

public:
     /** @name internalFunctionsPublic */
      //@{

         /** Constructor of CameraHal */
         CameraHal();

        /** Destructor of CameraHal
        *@todo Investigate why the destructor is virtual
        */
        virtual ~CameraHal();

        /** Creates singleton instance of CameraHal */
        static sp<CameraHardwareInterface> createInstance();

     //@}

/*--------------------Internal Member functions - Private---------------------------------*/
private:

        /** @name internalFunctionsPrivate */
        //@{

            /** Initialize CameraHal */
            status_t initialize();

            /** Deinitialize CameraHal */
            void deinitialize();

            /** Allocate preview buffers */
            status_t allocPreviewBufs(int width, int height, const char* previewFormat);

            /** Allocate image capture buffers */
            status_t allocImageBufs(int width, int height, const char* previewFormat);

            /** Free preview buffers */
            status_t freePreviewBufs();

            /** Free image bufs */
            status_t freeImageBufs();

            /** Initialize default parameters */
            void initDefaultParameters();

            /** Method to validate the size supported by CameraHal
              * @todo Currently this checks only for minimum size and for both IC and PRVIEW, change this
              *           method to something more generic and which checks for validity of all parameters
              */
            int validateSize(int w, int h);

            void dumpProperties(CameraProperties::CameraProperty** cameraProps);


        //@}




/*----------Member variables - Public ---------------------*/
public:
    notify_callback mNotifyCb;
    data_callback   mDataCb;
    data_callback_timestamp mDataCbTimestamp;
    void                 *mCallbackCookie;

    int32_t mMsgEnabled;
    bool mRecordEnabled;
    nsecs_t mCurrentTime;
    bool mFalsePreview;
    bool mPreviewEnabled;

    sp<CameraAdapter> mCameraAdapter;
    sp<AppCallbackNotifier> mAppCallbackNotifier;
    sp<DisplayAdapter> mDisplayAdapter;
    sp<MemoryManager> mMemoryManager;
    sp<CameraProperties> mCameraProperties;


    sp<IMemoryHeap> mPictureHeap;
    static wp<CameraHardwareInterface> singleton;

///static member vars


/*----------Member variables - Private ---------------------*/
private:
    mutable Mutex mLock;

    void* mCameraAdapterHandle;

    CameraParameters mParameters;
    sp<Overlay>  mOverlay;
    bool mPreviewRunning;

    int32_t *mImageBufs;
    int32_t *mPreviewBufs;

    ///@todo Rename this as preview buffer provider
    BufferProvider* mBufProvider;

    bool mPreviewBufsAllocatedUsingOverlay;

    CameraProperties::CameraProperty **mCameraPropertiesArr;
};


}; // namespace android

#endif