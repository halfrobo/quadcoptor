package com.quad.shubham.quad_app;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.support.v4.content.LocalBroadcastManager;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import com.jjoe64.graphview.GraphView;
import com.jjoe64.graphview.series.DataPoint;
import com.jjoe64.graphview.series.LineGraphSeries;

import java.util.ArrayList;

/**
 * Created by shubham on 18/6/15.
 */
public class Custom_graph_view extends LinearLayout {
    GraphView graph_view;
    ArrayList<LineGraphSeries> series_list;
    Context context;
    LinearLayout check_box_layout,top_layout;
    TextView view_port_label;
    EditText view_port_size_text;
    ArrayList<String> prefix_list;
    String graph_name;
    TextView graph_name_view;
    public int view_port_size=1000;
    boolean registered=false;
    final long update_delay=500;//500ms
    boolean update_now=true;
    int cur_max_x;
    boolean first_time=true;
    int h_m=30,v_m=5;

    BroadcastReceiver receiver=new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String data=intent.getStringExtra("data");
            String action=intent.getAction();
            if(action.length()>=Data_logging_service.intent_filter_prefix.length()
                    && action.substring(0,Data_logging_service.intent_filter_prefix.length())
                    .equals(Data_logging_service.intent_filter_prefix)) {

                String prefix=action.substring(Data_logging_service.intent_filter_prefix.length());
                int index=prefix_list.indexOf(prefix);
                if(index!=-1) {
                    if (data != null) {
                        String[] seperated = data.split(" ", 2);
                        if (seperated.length == 2) {
                            DataPoint point = new DataPoint(Integer.parseInt(seperated[0]), Integer.parseInt(seperated[1]));
                            series_list.get(index).appendDataWithoutUpdate(point,view_port_size);
                            cur_max_x=Integer.parseInt(seperated[0]);
                            if(update_now){
                                update_now=false;
                                Custom_graph_view.this.postDelayed(new Runnable() {
                                    @Override
                                    public void run() {
                                        try {
                                            update_now=true;
                                            graph_view.getViewport().setMinX(cur_max_x - view_port_size);
                                            graph_view.getViewport().setMaxX(cur_max_x);
                                            graph_view.onDataChanged(!first_time, false);
                                            first_time=false;
                                        } catch (Exception e) {
                                            Toast.makeText(Custom_graph_view.this.context.getApplicationContext(), e.getMessage(), Toast.LENGTH_SHORT).show();
                                        }
                                    }
                                },update_delay);
                            }
                        }
                    }
                }
            }
        }
    };

    public Custom_graph_view(Context _context, ArrayList<LineGraphSeries> _series_list, ArrayList<String> _prefix_list,String _graph_name) {
        super(_context);
        context=_context;
        series_list=_series_list;
        prefix_list=_prefix_list;
        graph_name=_graph_name;
        this.setOrientation(VERTICAL);

        top_layout=new LinearLayout(context);

        graph_name_view=new TextView(context);
        graph_name_view.setText(graph_name);

        view_port_label=new TextView(context);
        view_port_label.setText("View port size");

        view_port_size_text=new EditText(context);
        view_port_size_text.setInputType(InputType.TYPE_CLASS_NUMBER);
        view_port_size_text.setText(Data_store.get_attribute(context,
                Data_store.USER_SETTING_PREFIX + graph_name + ":view_port_size",
                Integer.toString(view_port_size)));

        view_port_size_text.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {

            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {

            }

            @Override
            public void afterTextChanged(Editable s) {
                view_port_size = try_parse_int(view_port_size_text.getText().toString());
                Data_store.set_attribute(context, Data_store.USER_SETTING_PREFIX + graph_name + ":view_port_size",
                        Integer.toString(view_port_size));
            }
        });

        top_layout.addView(graph_name_view,new LayoutParams(LayoutParams.WRAP_CONTENT,LayoutParams.WRAP_CONTENT,Gravity.LEFT));
        top_layout.addView(view_port_label, new LayoutParams(LayoutParams.WRAP_CONTENT,LayoutParams.WRAP_CONTENT,Gravity.RIGHT));
        top_layout.addView(view_port_size_text, new LayoutParams(LayoutParams.WRAP_CONTENT,LayoutParams.WRAP_CONTENT,Gravity.RIGHT));

        graph_view=new GraphView(context);
        graph_view.getViewport().setScalable(true);
        graph_view.getViewport().setScrollable(true);
        graph_view.getLegendRenderer().setVisible(true);

        check_box_layout=new LinearLayout(context);

        for(int i=0;i<series_list.size();i++){
            CheckBox temp_check_box=new CheckBox(context);
            temp_check_box.setText(series_list.get(i).getTitle());
            temp_check_box.setId(i + 1);
            temp_check_box.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                    int index=buttonView.getId() - 1;
                    LineGraphSeries series = series_list.get(index);
                    if (isChecked) {
                        Data_store.set_attribute(context,Data_store.USER_SETTING_PREFIX + graph_name+":"+series.getTitle(),"true");
                        if (graph_view.getSeries().indexOf(series) == -1) {
                            graph_view.addSeries(series);
                            register_receiver();
                        }
                    } else {
                        Data_store.set_attribute(context,Data_store.USER_SETTING_PREFIX + graph_name+":"+series.getTitle(),"false");
                        if (graph_view.getSeries().indexOf(series) != -1) {
                            graph_view.removeSeries(series);
                            register_receiver();
                        }
                    }
                }
            });

            //Do it after setOnCheckeListner so that it will add the series to graph too depending
            //on checked state
            temp_check_box.setChecked(Boolean.parseBoolean(Data_store
                    .get_attribute(context, Data_store.USER_SETTING_PREFIX + graph_name + ":" + series_list.get(i).getTitle(), "true")));

            check_box_layout.addView(temp_check_box);
        }

        register_receiver();

        this.addView(top_layout,new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, Gravity.NO_GRAVITY));
        this.addView(graph_view, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, Gravity.FILL));
        this.addView(check_box_layout, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, Gravity.NO_GRAVITY));
    }

    public void register_receiver(){
        unregister_receiver();
        IntentFilter filter=new IntentFilter();
        for(int i=0;i<check_box_layout.getChildCount();i++){
            if(((CheckBox)check_box_layout.getChildAt(i)).isChecked())
                filter.addAction(Data_logging_service.intent_filter_prefix+prefix_list.get(i));
        }
        LocalBroadcastManager.getInstance(context).registerReceiver(receiver,filter);
        registered=true;
    }

    public void unregister_receiver() {
        if(registered) {
            LocalBroadcastManager.getInstance(context).unregisterReceiver(receiver);
            registered = false;
        }
    }

    public int try_parse_int(String str){
        try{
            return Integer.parseInt(str);
        }catch (NumberFormatException e){
            return view_port_size;
        }
    }
}
