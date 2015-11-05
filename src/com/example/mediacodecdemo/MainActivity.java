package com.example.mediacodecdemo;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

//import android.support.v7.app.ActionBarActivity;
//import android.support.v4.app.Fragment;
import android.app.Activity;
import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;




public class MainActivity extends Activity implements SurfaceHolder.Callback {

	
	final private String TAG = "MediaCodecDemo";
	private String 	mMime = "video/avc"; //"video/mp4v-es";		
	private int 	mWidth = 800;
	private int		mHeight = 480;
	
	private SurfaceView mSurfaceView = null;
	private SurfaceHolder holder;
	public Surface mSurface;
	
	private PlayerThread mPlayerThread = null;
	
	
	public native int um_vdec_init(int codec, int width, int height);
	
	public native int um_vdec_decode(byte[] buf, int len);

	public native int um_vdec_fini();
	
	public native int um_vdec_setSurface();
	
	static {
		System.loadLibrary("mediacodec");
	}
	
	
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		LinearLayout tp = new LinearLayout(this);
		LayoutParams ltp = new LayoutParams(LayoutParams.WRAP_CONTENT,
				LayoutParams.WRAP_CONTENT);
		tp.setOrientation(1);
		
		
		//video decode test case Button 
		Button decode = new Button(this);
		decode.setText("test video decode");
		decode.setOnClickListener(new OnClickListener() {

			@Override
			public void onClick(View v) {
				// TODO Auto-generated method stub
				
				mPlayerThread.start();
			}

		});
		
		//video display surface 
		mSurfaceView = new SurfaceView(this);
		mSurfaceView.setVisibility(View.GONE);
		mSurfaceView.setVisibility(View.VISIBLE);
		holder = mSurfaceView.getHolder();
		holder.addCallback(this);
		holder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);	

		tp.addView(decode, ltp);
		tp.addView(mSurfaceView,ltp);
		setContentView(tp);	
		
		

	}
	
	
	protected void onDestroy(){
		
		super.onDestroy();
		mPlayerThread.release();
	}

	

	@Override
	public void surfaceCreated(SurfaceHolder holder) {
		// TODO Auto-generated method stub
		
		holder.setSizeFromLayout();
	}

	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width,
			int height) {
		// TODO Auto-generated method stub
		
		
		mSurface = holder.getSurface();
		
		um_vdec_setSurface();
		mPlayerThread = new PlayerThread( mSurface);
		
		
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder) {
		// TODO Auto-generated method stub
		
	}
	
	private int byteToInteger(byte[] buf) {
		int iCount = 0;

		for (int i = 0; i < 4; i++) {

			int n = (buf[i] < 0 ? (int) buf[i] + 256 : (int) buf[i] << (8 * i));
			iCount += n;
		}

		return iCount;
	}
	
	
	class PlayerThread extends Thread{
		
		private boolean run = true;
		private MediaCodec codec = null;
		
		
		PlayerThread(Surface surface){
			
			um_vdec_init(1, 800, 480);
			
			
//			MediaFormat format = MediaFormat.createVideoFormat(mMime, mWidth, mHeight);
//			format.setInteger(MediaFormat.KEY_BIT_RATE, 100000);
//			format.setInteger(MediaFormat.KEY_FRAME_RATE, 15);
//			
//			format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
//						MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar);
//					
//			String mime = format.getString(MediaFormat.KEY_MIME);
//			format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 5);  
//			
//			codec = MediaCodec.createDecoderByType(mime);
//			
//			Log.e(TAG, " create decode by type mime " + mime);
//			
//			if(codec == null){
//				
//				Log.e(TAG, " create decode by type fail");
//				return;
//			}
//			
//			codec.configure(format, surface, null, 0);
//			codec.start();
			
		}
		
		
		public void run(){
			
			
			InputStream fileIS = null;
//			ByteBuffer[] inputBuffers = codec.getInputBuffers();
//			ByteBuffer[] outputBuffers = codec.getOutputBuffers();
			
			byte[] bLen = new byte[4];
			byte[] bFrame = new byte[40960];
			int frameLen = 0;
			
			
			//fileIS = new FileInputStream("/mnt/sdcard/predecode.h264");
			try {
				
				fileIS = getResources().getAssets().open("predecode.h264");
			} catch (IOException e1) {
				// TODO Auto-generated catch block
				e1.printStackTrace();
			}
		
			Log.e(TAG,"run  thread");
			
			while (!Thread.currentThread().isInterrupted() && run) {
				Log.e(TAG,"run  thread 1 ");
				
				int bytes = 0;
				
				try {
					bytes = fileIS.read(bLen, 0, 4);
					Log.e(TAG,"run  thread 2 ");
				
					if(bytes == -1){
						
						Log.e(TAG, "release Thread.currentThread().interrupt()");
						Thread.currentThread().interrupt();
						run = false;
					}
					
					bytes = fileIS.read(bLen, 0, 4);
					frameLen = byteToInteger(bLen);
					
					bytes = fileIS.read(bFrame, 0, frameLen);
					Log.e(TAG,"bytesRead " + bytes);
				
				
				
					try {
						Thread.sleep(5);
					} catch (InterruptedException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
				
					um_vdec_decode(bFrame, frameLen);
					
//					if(bytes > 0){
//						
//						int inputBufferIndex = codec.dequeueInputBuffer(0);
//						
//						if(inputBufferIndex >= 0){
//							
//							ByteBuffer inputeBuffer = inputBuffers[inputBufferIndex];
//							inputeBuffer.clear();
//							inputeBuffer.put(bFrame, 0, frameLen);
//							
//							codec.queueInputBuffer(inputBufferIndex, 0, frameLen, 0, 0);
//						}
//						
//						BufferInfo info = new BufferInfo();
//						
//						int outputBufferIndex = codec.dequeueOutputBuffer(info, 0);
//						
//						if(outputBufferIndex >= 0){
//							
//							Log.e(TAG, "codec outputBufferIndex" + outputBufferIndex);
//							codec.releaseOutputBuffer(outputBufferIndex, true);
//							outputBufferIndex = codec.dequeueOutputBuffer(info, 0);
//						}else if (outputBufferIndex == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
//							
//							Log.e(TAG, "codec INFO_OUTPUT_BUFFERS_CHANGED");
//							outputBuffers = codec.getOutputBuffers();
//						} else if (outputBufferIndex == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
//						    
//							Log.e(TAG, "codec INFO_OUTPUT_FORMAT_CHANGED");
//							// Subsequent data will conform to new format.
//						    MediaFormat format = codec.getOutputFormat();
//						    
//						 }
//						
//					}
					
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
					
				}
			}
			
			
			try {
				if (fileIS != null){
					
					fileIS.close();
				}
				
				um_vdec_fini();
				
				Log.e(TAG, "release codec");
				codec.stop();
				codec.release();
				codec = null;
				
			} catch (IOException e) {
				e.printStackTrace();
			}
			
			
		}
		
		
		public void release(){
			
			run = false;
		}
	}

}
