
#include <DeckLinkAPI.h>

#include <g3log/g3log.hpp>

#include "libblackmagic/SDICameraControl.h"
#include "libblackmagic/OutputHandler.h"

namespace libblackmagic {

	OutputHandler::OutputHandler( IDeckLink *deckLink )
			:  _config( bmdModeHD1080p2997 ),								// Set a default
				_enabled(false),
				_deckLink(deckLink),
				_deckLinkOutput( nullptr ),
				_totalFramesScheduled(0),
				_buffer( new SharedBMSDIBuffer() ),
				_blankFrame( nullptr )
		{
			_deckLink->AddRef();
		}

	OutputHandler::~OutputHandler(void)
	{
		if( _deckLinkOutput ) _deckLinkOutput->Release();
		if( _deckLink ) _deckLink->Release();
	}


	IDeckLinkOutput *OutputHandler::deckLinkOutput()
	{
		if( !_deckLinkOutput ) {
			CHECK( S_OK == _deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&_deckLinkOutput) )
										<< "Could not obtain the IDeckLinkInput interface - result = %08x";
			CHECK(_deckLinkOutput != nullptr );
		}

		return _deckLinkOutput;
	}

	bool OutputHandler::enable()
	{

	  BMDVideoOutputFlags outputFlags  = bmdVideoOutputVANC;
	  HRESULT result;

	  BMDDisplayModeSupport support;
		IDeckLinkDisplayMode *displayMode = nullptr;

	  if( deckLinkOutput()->DoesSupportVideoMode( _config.mode(), 0, outputFlags, &support, &displayMode ) != S_OK) {
	    LOG(WARNING) << "Unable to find a query output modes";
	    return false;
	  }

	  if( support == bmdDisplayModeNotSupported ) {
	    LOG(WARNING) << "Display mode not supported";
	    return false;
	  }

	  // Enable video output
		LOG(INFO) << "Enabled output with mode " << displayModeToString(_config.mode()) << " (0x" << std::hex <<  _config.mode() << ") and flags " << outputFlags;
 		result = deckLinkOutput()->EnableVideoOutput(_config.mode(), outputFlags );
		if( result != S_OK ) {
	    LOG(WARNING) << "Could not enable video output, result = " << std::hex << result;
	    return false;
	  }

	  if( S_OK != displayMode->GetFrameRate( &_frameDuration, &_timeScale ) ) {
	    LOG(WARNING) << "Unable to get time rate information for output...";
	    return false;
	  }

		//LOG(INFO) << "Time value " << _timeValue << " ; " << _timeScale;

	  // Set the callback object to the DeckLink device's output interface
	  //_outputHandler = new OutputHandler( _deckLinkOutput, displayMode );
	  result = _deckLinkOutput->SetScheduledFrameCompletionCallback( this );
	  if(result != S_OK) {
	    LOGF(WARNING, "Could not set callback - result = %08x\n", result);
	    return false;
	  }

		_config.setMode( displayMode->GetDisplayMode() );
	  displayMode->Release();

		scheduleFrame( blankFrame() );

	  LOG(DEBUG) << "DeckLinkOutput initialized!";
		_enabled = true;
	  return true;
	}


	void OutputHandler::inputFormatChanged( BMDDisplayMode newMode )
	{
		LOG(INFO) << "Input mode has changed to " << displayModeToString(newMode);
	}

	// Callbacks for sending out new frames
	void OutputHandler::scheduleFrame( IDeckLinkVideoFrame *frame, uint8_t numRepeats )
	{
		LOG(DEBUG) << "Scheduling frame " << _totalFramesScheduled;
		deckLinkOutput()->ScheduleVideoFrame(frame, _totalFramesScheduled*_frameDuration,  _frameDuration*numRepeats, _timeScale );
		//deckLinkOutput()->ScheduleVideoFrame(frame, _totalFramesScheduled*_timeValue,  1, _timeScale );
		_totalFramesScheduled += numRepeats;
	}

	HRESULT	STDMETHODCALLTYPE OutputHandler::ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result)
	{
		BMDTimeValue frameCompletionTime = 0;
		CHECK( deckLinkOutput()->GetFrameCompletionReferenceTimestamp( completedFrame, _timeScale, &frameCompletionTime ) == S_OK);

		BMDTimeValue streamTime = 0;
		double playbackSpeed = 0;
		auto res = deckLinkOutput()->GetScheduledStreamTime(_timeScale, &streamTime, &playbackSpeed);


		LOG(DEBUG) << "Completed a frame at " << frameCompletionTime << " with result " << result << " ; " << res << " " << streamTime << " " << playbackSpeed;
		if( completedFrame != _blankFrame ) {
			LOG(DEBUG) << "Completed frame != _blankFrame";
			completedFrame->Release();
		}

		HRESULT r;

		_buffer->getReadLock();
		if( _buffer->buffer->len > 0 ) {
			LOG(INFO) << "Scheduling frame with " << int(_buffer->buffer->len) << " bytes of BM SDI Commands";
			r = deckLinkOutput()->ScheduleVideoFrame( makeFrameWithSDIProtocol( _deckLinkOutput, _buffer->buffer, true ),
		 																				streamTime, _frameDuration, _timeScale );
			//scheduleFrame( addSDIProtocolToFrame( _deckLinkOutput, _blankFrame, _buffer->buffer ) );
			bmResetBuffer( _buffer->buffer );
		} else {
			// Otherwise schedule a blank frame
			r = deckLinkOutput()->ScheduleVideoFrame( blankFrame(),
		 																						streamTime, _frameDuration, _timeScale );
		}
		_buffer->releaseReadLock();

		LOG_IF(WARNING, r != S_OK ) << "Scheduling not OK! " << result;

		// Can I release the completeFrame?

		return S_OK;
	}

bool OutputHandler::startStreams() {
		if( !_enabled && !enable() ) return false;

	LOG(DEBUG) << "Starting DeckLinkOutput streams ...";

	// // Pre-roll a few blank frames
	// const int prerollFrames = 3;
	// for( int i = 0; i < prerollFrames ; ++i ) {
	// 	scheduleFrame(blankFrame());
	// }

	HRESULT result = _deckLinkOutput->StartScheduledPlayback(0, _timeScale, 1.0);
	if(result != S_OK) {
		LOG(WARNING) << "Could not start video output - result = " << std::hex << result;
		return false;
	}

	return true;
}

bool OutputHandler::stopStreams()
{
	LOG(DEBUG) << "Stopping DeckLinkOutput streams";
	// // And stop after one frame
	BMDTimeValue actualStopTime;
	HRESULT result = deckLinkOutput()->StopScheduledPlayback(0, &actualStopTime, _timeScale);
	if(result != S_OK)
	{
		LOG(WARNING) << "Could not stop video playback - result = " << std::hex << result;
	}

	return true;
}


}
