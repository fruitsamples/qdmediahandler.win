//////////
//
//	File:		QDrawHandler.c
//
//	Contains:	Code for creating a derived media handler component for QuickDraw pictures.
//
//	Written by:	Tim Monroe
//				Based on MyMediaComponent by John Wang (see develop issue 14), with some
//				assistance from SampleMediaHandler by deeje cooley.
//
//	Copyright:	� 1993-1999 by Apple Computer, Inc., all rights reserved.
//
//	Change History (most recent first):
//
//	   <7>	 	02/25/99	rtm		added some comments
//	   <6>	 	01/28/99	rtm		fixed bug that caused garbage to be drawn when Copy menu item selected
//									or a drag of movie clipping attempted
//	   <5>	 	01/25/99	rtm		got Windows DLL working
//	   <4>	 	01/21/99	rtm		got both MacOS PPC and 68K versions working
//	   <3>	 	01/15/99	rtm		coordinated with SampleMediaHandler by deeje cooley
//	   <2>	 	01/05/99	rtm		revised coding style
//	   <1>	 	02/25/93	jw		first file; based on MyComponent shell
//
//	This project builds a derived (or custom) media handler. See the chapter "Derived Media Handler
//	Components" in the book Inside Macintosh: QuickTime Components for information about writing
//	derived media handler components. See also John Wang's article on derived media handlers in develop,
//	issue 14, for more information about the QuickDraw media handler. The main difference between his
//	code and the current sample code is that I've made the dispatching routines both PPC- and Windows-savvy,
//	using the ComponentDispatchHelper code. Also fixed a few bugs and added some more comments.
//
//////////


//////////
//
// header files
//
//////////

#include "QDrawHandler.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Component dispatch helper defines
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////

#if TARGET_CPU_68K
	#define COMPONENT_C_DISPATCHER
	#define COMPONENT_DISPATCH_MAIN
#endif

#define MEDIA_BASENAME() 				QDMH_
#define MEDIA_GLOBALS() 				QDMH_GlobalsHdl storage

#define CALLCOMPONENT_BASENAME()		MEDIA_BASENAME()
#define	CALLCOMPONENT_GLOBALS()			MEDIA_GLOBALS()

#define COMPONENT_UPP_PREFIX()			uppMedia
#define COMPONENT_SELECT_PREFIX()  		kMedia
#define COMPONENT_DISPATCH_FILE			"QDrawHandlerDispatch.h"

#define	GET_DELEGATE_COMPONENT()		((**storage).fDelegate)

#include "Components.k.h"
#include "MediaHandlers.k.h"
#include "ComponentDispatchHelper.c"


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Required component calls
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////
//
// QDMH_Open
// Open the derived media handler component.
//
//////////

PASCAL_RTN ComponentResult QDMH_Open (QDMH_GlobalsHdl storage, ComponentInstance theSelf)
{
#pragma unused(storage)

	QDMH_GlobalsHdl				myStorage = NULL;
	ComponentInstance			myComponent = NULL;
	
	// allocate the private global storage used by this component instance
	myStorage = (QDMH_GlobalsHdl)NewHandleClear(sizeof(QDMH_Globals));	
	if (myStorage == NULL)
		return(MemError());
	
	SetComponentInstanceStorage(theSelf, (Handle)myStorage);
	
	// open the base media handler component and target it
	myComponent = OpenDefaultComponent(MediaHandlerType, BaseMediaType);	
	if (myComponent == NULL)
		return(componentNotCaptured);
	
	ComponentSetTarget(myComponent, theSelf);

	(**myStorage).fDelegate = myComponent;
	(**myStorage).fSelf = theSelf;
	(**myStorage).fParent = theSelf;

	return(noErr);
}


//////////
//
// QDMH_Close
// Close the derived media handler component.
//
//////////

PASCAL_RTN ComponentResult QDMH_Close (QDMH_GlobalsHdl storage, ComponentInstance theSelf)
{
#pragma unused(theSelf)

	if (storage != NULL) {

		// close the base media handler component instance
		if ((**storage).fDelegate != NULL)
			CloseComponent((**storage).fDelegate);
		
		// dispose of the private global storage created by the _Open routine
		DisposeHandle((Handle)storage);
	}
	
	return(noErr);
}


//////////
//
// QDMH_Version
// Return the version of the derived media handler component.
//
//////////

PASCAL_RTN ComponentResult QDMH_Version (QDMH_GlobalsHdl storage)
{
#pragma unused(storage)

	return(kQDMH_Version);
}


//////////
//
// QDMH_Register
// Register.
//
// This routine is called once (usually at boot time) when the Component Manager first
// registers this component. Note that the cmpWantsRegisterMessage bit must be set in
// the component flags of the component in order for this routine to be called.
//
//////////

PASCAL_RTN ComponentResult QDMH_Register (QDMH_GlobalsHdl storage)
{
	if (storage != NULL)
		return(noErr);	// globals properly set up: the base media handler was targeted OK
	else
		return(-1);		// globals not properly set up: don't register
}


//////////
//
// QDMH_Target
// Target.
//
//////////

PASCAL_RTN ComponentResult QDMH_Target (QDMH_GlobalsHdl storage, ComponentInstance theTarget)
{
	// remember who is at the top of our calling chain
	(**storage).fParent = theTarget;
	
	// inform the base media handler of the change
	ComponentSetTarget((**storage).fDelegate, theTarget);
	
	return(noErr);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Derived media handler functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////
//
// QDMH_Initialize
// Initialize our media handler.
//
//////////

PASCAL_RTN ComponentResult QDMH_Initialize (QDMH_GlobalsHdl storage, GetMovieCompleteParams *theGMC)
{
	ComponentResult			myErr = noErr;
	long					myFlags;

	if ((storage == NULL) || (theGMC == NULL))
		return(paramErr);
		
	// set general characteristics
	(**storage).fMovie = theGMC->theMovie;
	(**storage).fTrack = theGMC->theTrack;
	(**storage).fMedia = theGMC->theMedia;
	(**storage).fCurMediaRate = theGMC->effectiveRate;
	MacSetRect(&((**storage).fGraphicsBox), 0, 0, (short)(theGMC->width >> 16), (short)(theGMC->height >> 16));
	(**storage).fTrackMatrix = theGMC->trackMovieMatrix;
	(**storage).fPort = theGMC->moviePort;
	(**storage).fDevice = theGMC->movieGD;
	(**storage).fSampleDescIndex = -1;

	// set media globals
	(**storage).fWhatChanged = kQDMHAllChanged;
	(**storage).fEnabled = false;
	(**storage).fNewMediaRate = theGMC->effectiveRate;
	(**storage).fPrevMediaTime = -1;
	
	// inform the base media handler of our capabilities:
	// * handlerHasSpatial indicates that we draw
	// * handlerNeedsBuffer indicates that we want QuickTime to maintain a drawing buffer for us
	myFlags = handlerHasSpatial | handlerNeedsBuffer;
	myErr = MediaSetHandlerCapabilities((**storage).fDelegate, myFlags, -1);

	return(myErr);
}


//////////
//
// QDMH_Idle
// Draw our media sample.
//
// Here is where we get time to process the current media sample data. In our case, we want to extract
// the QuickDraw picture data from the media and draw it into the current graphics port. If the sample
// data consisted of a complete QuickDraw picture, we could get that picture by calling GetMediaSample
// and then draw the picture by calling MacSetRect, TransformRect, and DrawPicture. (This is the route
// taken in the derived media handler described in Inside Macintosh: QuickTime Components, pp. 10-9 to
// 10-14.) Our media handler, however, supports key frames (also called sync frames), which complicate
// the work we need to do here. A key frame is a sample that does not rely on previous samples for any 
// of its information. A non-key frame (or difference frame) contains just data that has changed from
// the previous frame. 
//
//////////

PASCAL_RTN ComponentResult QDMH_Idle (QDMH_GlobalsHdl storage, TimeValue theMediaTime, long theFlagsIn, long *theFlagsOut, const TimeRecord *theMovieTime)
{
#pragma unused(theMovieTime)

	Media				myMedia = (**storage).fMedia;
	TimeValue			myPrevMediaTime = (**storage).fPrevMediaTime;
	long				myWhatChanged;			// flags indicating changes in media environment
	GDHandle			mySavedGD;
	CGrafPtr			mySavedPort;
	Rect				myDrawRect;
	Boolean				myRedraw;				// do we need to draw anything?
	Boolean				myIsDone;
	TimeValue			myTime;
	long				mySize;
	PicHandle			mySyncPic = NULL;
	PicHandle			myCurrPic = NULL;
	TimeValue			mySyncSampleTime;
	TimeValue			myCurrSampleTime;
	long				myCurrSampleIndex;
	OSErr				myErr = noErr;

	//////////
	//
	// inspect theFlagsIn to determine what the Movie Toolbox wants us to do
	//
	//////////

	// don't draw anything if mPreflightDraw flag is set
	if (theFlagsIn & mPreflightDraw)
		return(myErr);

	//////////
	//
	// initialize
	//
	//////////

	// allocate space for two pictures (the current sample's picture and the previous sync frame);
	// GetMediaSample resizes these handles as necessary when returning the media data
	mySyncPic = (PicHandle)NewHandle(sizeof(Picture));
	myCurrPic = (PicHandle)NewHandle(sizeof(Picture));
	if ((mySyncPic == NULL) || (myCurrPic == NULL)) {
		myErr = memFullErr;
		goto bail;
	}
	
	GetGWorld(&mySavedPort, &mySavedGD);
	SetGWorld((GWorldPtr)(**storage).fPort, (**storage).fDevice);
	
	//////////
	//
	// get information about the current sample
	//
	//////////

	myErr = GetMediaSample(myMedia, (Handle)myCurrPic, 0, NULL, theMediaTime, &myCurrSampleTime, NULL, NULL, &myCurrSampleIndex, 0, NULL, NULL);
	if (myErr != noErr)
		goto bail;
	
	// check to see whether the sample description index has changed since the last sample
	if (myCurrSampleIndex != (**storage).fSampleDescIndex) {
		(**storage).fSampleDescIndex = myCurrSampleIndex;
		(**storage).fWhatChanged |= kQDMHSampleDescChanged;
	}

	//////////
	//
	// determine what, if anything, in the media has changed
	// (we don't need to draw the current media sample if nothing has changed)
	//
	//////////

	myRedraw = false;
	myWhatChanged = (**storage).fWhatChanged;
	
	if (myWhatChanged != kQDMHNothingChanged) {

		if (myWhatChanged & kQDMHSetActive)		// if media was just enabled, then redraw; else don't change
			myRedraw = (**storage).fEnabled;

		if (myWhatChanged & kQDMHSetRate) {
			// if we are now playing in the opposite direction, then redraw
			if ((((**storage).fCurMediaRate < 0) && ((**storage).fNewMediaRate > 0))
			 || (((**storage).fCurMediaRate > 0) && ((**storage).fNewMediaRate < 0)))
				myRedraw = true;
			(**storage).fCurMediaRate = (**storage).fNewMediaRate;
		}
		
		if (myWhatChanged & kQDMHTrackEdited)
			myRedraw = true;					// if track edited, then redraw
		
		if (myWhatChanged & kQDMHSetGWorld)
			myRedraw = true;					// if new GWorld, then redraw
		
		if (myWhatChanged & kQDMHSetDimensions)
			myRedraw = true;					// if new dimensions, then redraw
		
		if (myWhatChanged & kQDMHSetMatrix)
			myRedraw = true;					// if new matrix, then redraw
		
		if (myWhatChanged & kQDMHSampleDescChanged) {
			QDrawDescriptionHandle		myQDDesc;
			
			// the sample description has changed; make sure it's a version we can handle
			myQDDesc = (QDrawDescriptionHandle)NewHandleClear(sizeof(QDrawDescription));
			if (myQDDesc != NULL) {
				GetMediaSampleDescription(myMedia, (**storage).fSampleDescIndex, (SampleDescriptionHandle)myQDDesc);
#if HANDLER_SWAPS_SAMPLE_DESC
				if ((**myQDDesc).version > kQDMH_Version)
#else
				if ((**myQDDesc).version > EndianU32_NtoB(kQDMH_Version))
#endif
					(**storage).fEnabled = false;
				DisposeHandle((Handle)myQDDesc);
			} else {
				(**storage).fEnabled = false; 
			}
		}
		
		// clear out flags indicating changes in media environment
		(**storage).fWhatChanged = kQDMHNothingChanged;
	}
	
	// if we are playing the movie backwards, we must always redraw from last sync frame
	if ((**storage).fCurMediaRate < 0)
		myRedraw = true;
	
	//////////
	//
	// we do not need to draw anything if myRedraw is false and if myPrevMediaTime == myCurrSampleTime
	// (since we've already drawn that sample and nothing has occurred to cause us to have to redraw it)
	//
	// otherwise, we do need to draw; what we draw depends on the value of myRedraw:
	//  (a) if myRedraw = true, redraw everything since the previous key frame
	//	(b) if myRedraw = false, redraw everything since the previous key frame or the previous media frame,
	//		whichever is closer to current frame
	//
	//////////

	if (myRedraw || (myPrevMediaTime != myCurrSampleTime)) {
	
		// find the previous key frame; note that we use the nextTimeEdgeOK flag, since the current frame
		// might be a key frame; note also that if the media doesn't contain any key frames, the sample
		// time returned in mySyncSampleTime is the same as myCurrSampleTime (that is, every frame is a key
		// frame)
		
		GetMediaNextInterestingTime(myMedia, nextTimeSyncSample + nextTimeEdgeOK, myCurrSampleTime, -1, &mySyncSampleTime, NULL);
		
		// if myRedraw = false, mySyncSampleTime <= myPrevMediaTime, and myCurrSampleTime is ahead of
		// myPrevMediaTime, then search to set the place to draw as the sample after myPrevMediaTime
		if (!myRedraw && (mySyncSampleTime <= myPrevMediaTime) && (myPrevMediaTime < myCurrSampleTime)) {
			myTime = mySyncSampleTime;
			
			while ((myTime >= 0) && (myTime < myPrevMediaTime))
				GetMediaNextInterestingTime(myMedia, nextTimeMediaSample, myTime, 1, &myTime, NULL);

			if ((myTime == myPrevMediaTime) && (myTime != -1)) {
				GetMediaNextInterestingTime(myMedia, nextTimeMediaSample, myTime, 1, &myTime, NULL);
				if (myTime != -1)
					mySyncSampleTime = myTime;	
			}
		}
		
	//////////
	//
	// draw the picture, beginning at mySyncSampleTime; but don't draw if the media is disabled
	//
	//////////

		myIsDone = false;
		myTime = mySyncSampleTime;
		while (!myIsDone && (**storage).fEnabled) {
		
			if (myTime == myCurrSampleTime)
				myIsDone = true;
				
			myErr = GetMediaSample(myMedia, (Handle)mySyncPic, 0, &mySize, myTime, NULL, NULL, NULL, NULL, 0, NULL, NULL);
			if (myErr != noErr)
				goto bail;
				
			myDrawRect = (**storage).fGraphicsBox;
			TransformRect(&(**storage).fTrackMatrix, &myDrawRect, NULL);
			DrawPicture(mySyncPic, &myDrawRect);
			if (!myIsDone) {
				GetMediaNextInterestingTime(myMedia, nextTimeMediaSample, myTime, 1, &myTime, NULL);
				if (myTime < 0)
					myIsDone = true;
			}
		}
		
		// say we drew somthing
		*theFlagsOut |= mDidDraw;
	}
	
	// update the previous media time
	(**storage).fPrevMediaTime = myCurrSampleTime;
		
	//////////
	//
	// clean up and return
	//
	//////////
	
bail:
	SetGWorld((GWorldPtr)mySavedPort, mySavedGD);
	
	if (mySyncPic != NULL)
		DisposeHandle((Handle)mySyncPic);
	if (myCurrPic != NULL)
		DisposeHandle((Handle)myCurrPic);
	
	return(myErr);
}


//////////
//
// QDMH_SetActive
// Set the enabled state of the media.
//
//////////

PASCAL_RTN ComponentResult QDMH_SetActive (QDMH_GlobalsHdl storage, Boolean theEnableMedia)
{
	if ((**storage).fEnabled != theEnableMedia) {
		(**storage).fEnabled = theEnableMedia;
		(**storage).fWhatChanged |= kQDMHSetActive;
	}
	
	return(noErr);
}


//////////
//
// QDMH_SetRate
// Set the media rate.
//
//////////

PASCAL_RTN ComponentResult QDMH_SetRate (QDMH_GlobalsHdl storage, Fixed theRate)
{
	// save the new rate in fNewMediaRate so that we can compare with previous rate;
	// if the new rate is in the same direction, we won't want to redraw again from the previous key frame
	if ((**storage).fNewMediaRate != theRate) {
		(**storage).fNewMediaRate = theRate;
		(**storage).fWhatChanged |= kQDMHSetRate;
	}
	
	return(noErr);
}


//////////
//
// QDMH_TrackEdited
// Set the track-edited state.
//
//////////

PASCAL_RTN ComponentResult QDMH_TrackEdited (QDMH_GlobalsHdl storage)
{
	(**storage).fWhatChanged |= kQDMHTrackEdited;
	
	return(noErr);
}


//////////
//
// QDMH_SetGWorld
// Set the media graphics port.
//
//////////

PASCAL_RTN ComponentResult QDMH_SetGWorld (QDMH_GlobalsHdl storage, CGrafPtr thePort, GDHandle theGD)
{
	(**storage).fPort = thePort;
	(**storage).fDevice = theGD;
	(**storage).fWhatChanged |= kQDMHSetGWorld;
	
	return(noErr);
}


//////////
//
// QDMH_SetDimensions
// Set the media dimensions.
//
//////////

PASCAL_RTN ComponentResult QDMH_SetDimensions (QDMH_GlobalsHdl storage, Fixed theWidth, Fixed theHeight)
{
	MacSetRect(&((**storage).fGraphicsBox), 0, 0, (short)(theWidth >> 16), (short)(theHeight >> 16));
	(**storage).fWhatChanged |= kQDMHSetDimensions;

	return(noErr);
}


//////////
//
// QDMH_SetMatrix
// Set the track or movie matrix.
//
//////////

PASCAL_RTN ComponentResult QDMH_SetMatrix (QDMH_GlobalsHdl storage, MatrixRecord *theTrackMovieMatrix)
{	
	// don't cause unnecessary updates if the matrix doesn't really change
	// (this can happen if the resize button is clicked on, but not moved)
	if (!EqualMatrix(&((**storage).fTrackMatrix), theTrackMovieMatrix)) {
		(**storage).fTrackMatrix = *theTrackMovieMatrix;
		(**storage).fWhatChanged |= kQDMHSetMatrix;
	}

	return(noErr);
}


//////////
//
// QDMH_SampleDescriptionChanged
// Handle changes to the sample description tables.
//
//////////

PASCAL_RTN ComponentResult QDMH_SampleDescriptionChanged (QDMH_GlobalsHdl storage, long theIndex)
{
	// the sample description tables store info such as data version
	(**storage).fSampleDescIndex = theIndex;
	(**storage).fWhatChanged |= kQDMHSampleDescChanged;

	return(noErr);
}


#if HANDLER_SWAPS_SAMPLE_DESC
//////////
//
// QDMH_SampleDescriptionB2N
// Convert our sample description from big- to native-endian format.
//
//////////

PASCAL_RTN ComponentResult QDMH_SampleDescriptionB2N (QDMH_GlobalsHdl storage, SampleDescriptionHandle theSampleDesc)
{
#pragma unused(storage)

	QDrawDescriptionHandle		myQDDesc = (QDrawDescriptionHandle)theSampleDesc;
	
	if ((myQDDesc == NULL) || (*myQDDesc == NULL))
		return(paramErr);

	// flip any fields in our sample description that are not in the SampleDescription structure
	(**myQDDesc).version = EndianU32_BtoN((**myQDDesc).version);
	
	return(noErr);
}


//////////
//
// QDMH_SampleDescriptionN2B
// Convert our sample description from native- to big-endian format.
//
//////////

PASCAL_RTN ComponentResult QDMH_SampleDescriptionN2B (QDMH_GlobalsHdl storage, SampleDescriptionHandle theSampleDesc)
{
#pragma unused(storage)

	QDrawDescriptionHandle		myQDDesc = (QDrawDescriptionHandle)theSampleDesc;

	if ((myQDDesc == NULL) || (*myQDDesc == NULL))
		return(paramErr);

	// flip any fields in our sample description that are not in the SampleDescription structure
	(**myQDDesc).version = EndianU32_NtoB((**myQDDesc).version);
	
	return(noErr);
}
#endif	// HANDLER_SWAPS_SAMPLE_DESC
