package com.example.h264dec;

import java.io.File;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;


public class ActivityH264Decoder extends Activity implements OnClickListener  {	
	private int _filePictureWidth;
	private int _filePictureHeight;
	
	private String _fileName = null;
	public static boolean decodeFormatIsRgb = false;
	private static String TAG = "Activity Decoder";
	    
    /** Called when the activity is first created. */
	Button _buttonDecode, _buttonOpenFileViewer;
	TextView _tvDecoderInput, _tvDecoderOutput, _tvDecoderResult, _tvDecoderHeader;

	MyNDKDecodeThread _threadDecode;

	private String outputFile() {
		return _fileName + (decodeFormatIsRgb ? ".dec.rgb" : ".dec.yuv");
	}
	
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

		_fileName = getIntent().getStringExtra("h264.fileName");
		int width = 320;
    	int height = 240;
    	String[] parts = _fileName.split("\\.");
    	for (int i = 0; i < parts.length; i++) {
    		if (parts[i].contains("x")) {
    			String[] dims = parts[i].split("x");
    			if (dims.length == 2) {
    				try {
	    				width = Integer.valueOf(dims[0]).intValue();
	    				height = Integer.valueOf(dims[1]).intValue();
	    				if (width != 0 && height != 0) {
	    					break;
	    				}
    				} catch (Exception e) {
    					// Filename may have had an 'x' in the title
    					// that did not specify dimensions.
    				}
    			}
    		}
    	}
    	_filePictureWidth = width;
    	_filePictureHeight = height;
			
		setContentView(R.layout.decoder);

		_buttonOpenFileViewer = (Button)findViewById(R.id.buttonOpenYuv);
		_buttonOpenFileViewer.setOnClickListener(this);
		_buttonOpenFileViewer.setEnabled(false);
		
		_buttonDecode = (Button)findViewById(R.id.buttonDecode);
		_buttonDecode.setOnClickListener(this);

		_tvDecoderHeader = (TextView)findViewById(R.id.decoderHeader);
		
		_tvDecoderInput = (TextView)findViewById(R.id.tvInputFile);
        _tvDecoderInput.setText("Input File: " + _fileName);

        _tvDecoderOutput = (TextView)findViewById(R.id.tvOutputFile);
        _tvDecoderOutput.setText("Output File: " + outputFile());

        _tvDecoderResult = (TextView)findViewById(R.id.tvStatus);
		_tvDecoderResult.setText("Ready to Decode");
		
        	if (decodeFormatIsRgb) {
        		_tvDecoderHeader.setText(R.string.headerH264ToRgb);
        		_buttonOpenFileViewer.setText(R.string.openRgb);
        	} else {
        		_tvDecoderHeader.setText(R.string.headerH264ToYuv);
        		_buttonOpenFileViewer.setText(R.string.openYuv);        		
        	}			
    }

	public void onDestroy() {
		super.onDestroy();

		if (_threadDecode != null) {
			_threadDecode = null;
		}
	}

	@Override
	public void onClick(View v) {
		if (v == _buttonDecode) {
			_buttonDecode.setEnabled(false);
			_tvDecoderResult.setText("Decoding...");
			_threadDecode = new MyNDKDecodeThread();
			_threadDecode.start();
		}
		
		if (v == _buttonOpenFileViewer) {
			Intent resultIntent = new Intent(this, ActivityYuvOrRgbViewer.class);
        	resultIntent.putExtra("h264.fileName", outputFile());
			startActivity(resultIntent);
		}
	}
	
	public native String decodeFile(String inFile, String outFile, int width, int height);

	Handler hRefresh = new Handler(){
		@Override
		public void handleMessage(Message msg) {
		switch(msg.what){
		     case 0:
		    	 _tvDecoderResult.setText((String)msg.obj);
		    	 File file = new File(MainActivity.getPathInStorageDirectory(outputFile()));
		    	 if (file.exists() && file.canRead()) {
		    		 _buttonOpenFileViewer.setEnabled(true);
		    	 } else {
			    	 _tvDecoderResult.append(" File cannot be read.");
		    	 }
		         break;
		     case 1:
		    	 Intent resultIntent = new Intent(ActivityH264Decoder.this, ActivityYuvOrRgbViewer.class);
					startActivity(resultIntent);
		    	 break;
		     }
		}
	};

	private class MyNDKDecodeThread extends Thread {
		public void run() {
			Message msg = new Message();
			msg.what = 1;
			hRefresh.sendMessage(msg);
			try {
				Thread.sleep(3000);
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
        	String inputPath = MainActivity.getPathInStorageDirectory(_fileName);
        	String outputPath = MainActivity.getPathInStorageDirectory(outputFile());
			
        	String result = decodeFile(inputPath, outputPath, _filePictureWidth, _filePictureHeight);
        	msg.what = 0;
			msg.obj = result;
			hRefresh.sendMessage(msg);
		}
	}
}
