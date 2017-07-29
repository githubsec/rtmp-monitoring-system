/* ======================================================================
*  Copyright ï¿?2013 Qualcomm Technologies, Inc. All Rights Reserved.
*  QTI Proprietary and Confidential.
*  =====================================================================
*
*  Licensed by Qualcomm, Inc. under the Snapdragon SDK for Android license.
*
*  You may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*    https://developer.qualcomm.com/snapdragon-sdk-license
*
*  This Qualcomm software is supplied to you by Qualcomm Inc. in consideration
*  of your agreement to the licensing terms.  Your use, installation, modification
*  or redistribution of this Qualcomm software constitutes acceptance of these terms.
*  If you do not agree with these terms, please do not use, install, modify or
*  redistribute this Qualcomm software.
*
*  Qualcomm grants you a personal, non-exclusive license, under Qualcomm's
*  copyrights in this original Qualcomm software, to use, reproduce, modify
*  and redistribute the Qualcomm Software, with or without modifications, in
*  source and/or binary forms; provided that if you redistribute the Qualcomm
*  Software in its entirety and without modifications, you must retain this
*  notice and the following text and disclaimers in all such redistributions
*  of the Qualcomm Software.
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*
* @file    VideoDisplayView.java
* @brief
*
*/
package com.example.h264dec;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.ByteBuffer;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.ImageFormat;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.YuvImage;
import android.os.AsyncTask;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class VideoDisplayView extends SurfaceView implements SurfaceHolder.Callback {
	private MyTask _myTask;

	private boolean _surfaceIsReady;
	private String _displayFilePath;
	private SurfaceHolder _surfaceHolder;
	private static final String TAG = "VideoDisplayView";

	public int previewHeight = 320;
	public int previewWidth = 240;
	
	private boolean isRgbFormat = false;

	public VideoDisplayView(Context context) {
		super(context);
	}

	public VideoDisplayView(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	public VideoDisplayView(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
	}


	@Override
	protected void onFinishInflate() {
		super.onFinishInflate();
        getHolder().addCallback(this);
	}

	@Override
	protected void onDraw(Canvas canvas) {
	}

	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width,
			int height) {
	}

	@Override
	public void surfaceCreated(SurfaceHolder holder) {
		_surfaceIsReady = true;
		_surfaceHolder = holder;
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder) {
	}

	public void setDisplayFilePath(String displayFilePath) { 
		_displayFilePath = displayFilePath;
		if (displayFilePath.endsWith("rgb")) {
			isRgbFormat = true;
		} else { // "yuv"
			isRgbFormat = false;			
		}
	}

	public String getDisplayFilePath() { 
		return _displayFilePath;
	}

	
	public boolean surfaceIsReady() {
		return _surfaceIsReady;
	}

	public void startDisplaying() {
		if (_myTask != null && !_myTask.isCancelled() && _myTask.getStatus() == AsyncTask.Status.RUNNING) {
			return;			
		}
		_myTask = new MyTask();
		_myTask.execute(_displayFilePath);
	}

	public void stopDisplaying() {
		if (_myTask != null) {
			_myTask.cancel(true);
		}
		_myTask = null;
	}

	Bitmap getBitmapFromYuv(byte[] data) {
		YuvImage yuv = new YuvImage(data, ImageFormat.NV21,
									previewWidth, previewHeight, null);

		ByteArrayOutputStream baos = new ByteArrayOutputStream();
		yuv.compressToJpeg(new Rect(0, 0, previewWidth, previewHeight), 50, baos);
		Bitmap bmp = BitmapFactory.decodeByteArray(baos.toByteArray(), 0, baos.size());

		Matrix matrix = new Matrix();
 
        // recreate the new Bitmap
        Bitmap resizedBitmap = Bitmap.createBitmap(	bmp, 0, 0,
        											previewWidth, previewHeight, matrix, true);
		return resizedBitmap;
	}

	Bitmap getBitmapFromRgb(byte[] data) {
		Bitmap bitmap = Bitmap.createBitmap(previewWidth, previewHeight, Bitmap.Config.RGB_565);
		ByteBuffer buffer = ByteBuffer.wrap(data);
		bitmap.copyPixelsFromBuffer(buffer);
		return bitmap;
	}
	
	public void drawCanvasBitmap(Canvas c, Bitmap bmp) {
		int sourceWidth = previewWidth;
		int sourceHeight = previewHeight;
		
		// Get the aspect ratio
		float zoom = Math.min((float)this.getWidth()/sourceWidth, (float)this.getHeight()/sourceHeight);
		int displayWidth = (int)(zoom * sourceWidth);
		int displayHeight = (int)(zoom * sourceHeight);

		int x = (int)((getWidth() - displayWidth) / 2);
		int y = (int)((getHeight() - displayHeight) / 2);
		if (c != null && bmp != null && this != null)
			
			c.drawBitmap(bmp, 
					new Rect(0, 0, bmp.getWidth(), bmp.getHeight()), 
					new Rect(x, y, x + displayWidth, y + displayHeight), 
					null);
	}

	private class MyTask extends AsyncTask<String, Bitmap, String> {
		private byte[] getBytesFromFIS(FileInputStream fis, 
									   File file, 
									   int offsetStart, 
									   int length) throws IOException {
		    // Get the size of the file
		    long filelength = file.length();

		    // You cannot create an array using a long type.
		    // It needs to be an int type.
		    // Before converting to an int type, check
		    // to ensure that file is not larger than Integer.MAX_VALUE.
		    if (filelength > Integer.MAX_VALUE) {
		        // File is too large
		    }

		    // Create the byte array to hold the data
		    byte[] bytes = new byte[length];

		    // Read in the bytes
		    int numRead = 0;
		    int offset = 0;

		    while (offset < bytes.length
		           && (numRead=fis.read(bytes, offset, bytes.length-offset)) >= 0) {
		        offset += numRead;
		    }

		    // Ensure all the bytes have been read in
		    if (offset < bytes.length) {
		        throw new IOException("Could not completely read file "+file.getName());
		    }
		    return bytes;
		}
		
		protected void onCancelled() {
	    }

		protected void onPostExecute(String result) {
	    }

		protected void onProgressUpdate (Bitmap... values) {
			if (_surfaceHolder != null) {
				Canvas c = _surfaceHolder.lockCanvas();
				drawCanvasBitmap(c, values[0]);
				if (c != null) {
					_surfaceHolder.unlockCanvasAndPost(c);
				}
			}
		}

		@Override
		protected String doInBackground(String... params) {
			String displayFilePath = params[0];

			File file = null;
			FileInputStream fis = null;
			int length = isRgbFormat ? previewWidth * previewHeight * 2 :
							(int)(previewWidth * previewHeight * 3 / 2);
			int offset = 0;
			while (true) {
				if (fis == null && displayFilePath != null) {
					file = new File(displayFilePath);
					try {
						fis = new FileInputStream(file);
					} catch (FileNotFoundException e) {
						Log.e(TAG, "File not found: " + e.getLocalizedMessage());
					}
				}
				
				if (fis != null) {
					try {
						if (offset + length >= file.length()) {
							fis.close();
							
							fis = null;
							fis = new FileInputStream(file);
							offset = 0;
						}

						byte[] data = this.getBytesFromFIS(fis, file, offset, length);

						if (isRgbFormat) {
							Bitmap bmp = getBitmapFromRgb(data);
							this.publishProgress(bmp);		

						} else { // NV12
							// Data should be in NV12, need to convert to NV21
							int frameLength = data.length;
							for (int i = (frameLength * 2) / 3; i < frameLength; i += 2) {
								byte tmp = data[i];
								data[i] = data[i+1];
								data[i+1] = tmp;
							}
	
							Bitmap bmp = getBitmapFromYuv(data);
							this.publishProgress(bmp);
						}

						offset += length;
												
					} catch (IOException e) {
						Log.e(TAG, "IOException in  get bytes from file: " + e.getLocalizedMessage());
					}

					try {
						Thread.sleep(100/3);
					} catch (Exception e) { break; }

				} else {
					try {
						Thread.sleep(2000);
					} catch (Exception e) { break; }
				}
			}
			if (fis != null) {
				try {
					fis.close();
				} catch (Exception e) {}
				fis = null;
			}
			return null;
		}
	}
}
