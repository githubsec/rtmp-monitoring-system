package com.example.h264dec;

import java.io.File;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.Timer;
import java.util.TimerTask;
import java.util.Vector;




import android.os.Bundle;
import android.os.Environment;
import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.hardware.Camera;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.AdapterView.OnItemClickListener;

public class MainActivity extends Activity implements OnItemClickListener {
	private TextView _textViewPrompt;
	private ListView _listView;
	private static final String TAG = "MENU SELECTOR";
	static{
		try {
	    	System.loadLibrary("avutil-54");
	    	System.loadLibrary("swresample-1");
	    	System.loadLibrary("avcodec-56");
	    	System.loadLibrary("avformat-56");
	    	System.loadLibrary("swscale-3");
	    	System.loadLibrary("postproc-53");
	    	System.loadLibrary("avfilter-5");
	    	System.loadLibrary("avdevice-56");
			//System.loadLibrary("avutil-linphone-arm");
			//System.loadLibrary("swscale-linphone-arm");
			//System.loadLibrary("avcodec-linphone-arm");
			System.loadLibrary("ndk_h264dec");
	
		} catch (UnsatisfiedLinkError e) {
			e.printStackTrace();
		}
	}
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);
        Button startButton = (Button) this.findViewById(R.id.button_start);
		//final EditText urlEdittext_input= (EditText) this.findViewById(R.id.input_url);
		final EditText urlEdittext_output= (EditText) this.findViewById(R.id.output_url);
		//startButton.setOnClickListener(this);
		startButton.setOnClickListener(new OnClickListener() {
			public void onClick(View arg0){
				rtmpurl = urlEdittext_output.getText().toString();
				Intent resultIntent = null;
				resultIntent = new Intent(MainActivity.this, ActivityYuvOrRgbViewer.class);

				String fileName =urlEdittext_output.getText().toString();
	        	resultIntent.putExtra("h264.fileName", getPathInStorageDirectory(fileName));
				startActivity(resultIntent);
			}
		});
		
        
	}
	private static String storagePath = Environment.getExternalStorageDirectory().getPath() +  "/h264";
	private static String rtmpurl; 
	public static String getPathInStorageDirectory(String path) {
		return rtmpurl;
	}
	
	public static String getStorageDirectory() {
		return storagePath;
	}

	public static boolean validateStorageDirectory() {
		File dir = new File(getStorageDirectory());
		if (dir.exists() || dir.mkdir()) {
        	return true;
		}
        return false;
	}
	int backCnt = 0;
	private Timer exitTimer = null;
	private static final int EXIT_TIMEOUT = 3*1000;//3s
	
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if(keyCode == KeyEvent.KEYCODE_BACK ){
            	if(backCnt++ == 0){
    				Toast.makeText(this, "come back", Toast.LENGTH_SHORT).show();
    				  if(exitTimer!=null){
    					  exitTimer.cancel();
    					  exitTimer = null;
    					}
    				  exitTimer = new Timer();
    				  exitTimer.schedule(new TimerTask() {
    					
    					@Override
    					public void run() {
    						// TODO Auto-generated method stub
    						backCnt = 0;
    					}
    				}, EXIT_TIMEOUT);
    			}else{
    				//LogHelper.logPrint(RegisterActivity.class, "stop services return "+ stopService(new Intent(getApplicationContext(), RegisterServices.class)));
    				android.os.Process.killProcess(android.os.Process.myPid());
    			}
    			return true;
			
		}else{
			return super.onKeyDown(keyCode, event);
		}
	}
	@Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        Log.i("MainActivity", "onConfigurationChanged");
        if(newConfig.orientation==Configuration.ORIENTATION_LANDSCAPE){
           Log.d("MainActivity","current screen is portrait");
        }else{
        Log.d("MainActivity","current screen is landscape");
        }
    }
	public void onItemClick(AdapterView<?> arg0, View view, int arg2, long arg3) {

			Intent resultIntent = null;
			resultIntent = new Intent(this, ActivityYuvOrRgbViewer.class);


			TextView tView = (TextView)view;
			String fileName = tView.getText().toString();
        	resultIntent.putExtra("h264.fileName", getPathInStorageDirectory(fileName));
			startActivity(resultIntent);
			return;

	}
}
