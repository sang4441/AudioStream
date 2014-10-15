package com.example.audiostream;

import com.example.audiostream.R;

import android.support.v7.app.ActionBarActivity;
import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;


public class MainActivity extends Activity {
 
	private boolean isSendering;
	private static int BUFFER_FRAME_SIZE = 16000;
	short[] lin = new short [BUFFER_FRAME_SIZE];
	short[] inbuffer  = new short[64];
	short[] outbuffer = new short[128];
	     
    @Override     
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);     
             
        createEngine();            
        createAudioRecorder();		
		createBufferQueueAudioPlayer();
        
        ((Button) findViewById(R.id.start)).setOnClickListener(new OnClickListener() {
            public void onClick(View view) {
            	
            	isSendering = true;
            	Log.d("start", "start button");
            		 
            	Thread t = new Thread() { 
            		@Override
            		 public void run() {             			            	            			                         			                            			                           			 
            			Log.d("start", "record about to begin1");
            			lin = startRecording();
                    	playBack(lin, BUFFER_FRAME_SIZE);    
 
            			Log.d("start", "record about to begin3");       	  		            			 
            	    }
            	};
            	t.start();                 	
            }
        });    
                     
             
        ((Button) findViewById(R.id.stop)).setOnClickListener(new OnClickListener() {
            public void onClick(View view) {
            	Log.d("stop", "stop button");
            	Log.d("stop", "stop button finished"); 
            	isSendering = false;       

				Log.d("start", "played");
            }
        });              
    }
     
   
 
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.main, menu);
        return true;
    } 

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();
        if (id == R.id.action_settings) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }  
    
    static {  
        System.loadLibrary("my_lib");  
    }  
    public static native void createEngine();
    public static native void createBufferQueueAudioPlayer();
    public static native void playBack(short[] lin, int decodeSize);
    public static native boolean createAudioRecorder();
    public static native short[] startRecording(); 
    public static native short[] getBuffer();
    public static native void setBuffer(short[] lin, int decodeSize);       
}

