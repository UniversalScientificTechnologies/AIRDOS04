

pcb_width = 85.86 + 0.4;
pcb_thickness = 1.6+0.2;
pcb_above_ground = 12.6; 

pcb_rail_thickness = 1.6+1.5+1.5+0.1;
pcb_rail_depth = 1.5;

albase_length = 162;
box_width = 95-0.5;

flange_width = 20;
flange_th = 3.5;
mounting_holes_distance = 3*10.16;

M3_screw_diameter = 3.25;
M3_nut_diameter = 6.4;


module pcb_edge(){
    difference(){
        translate([0, 0, 1.5]) cube([136.6+1, pcb_rail_thickness-0.2, 3], center=true);
        translate([-1, 0, 2+pcb_rail_depth-0.1]) cube([136.6+1, 1.65, 4], center=true);
        translate([0, pcb_rail_thickness/2, 0]) rotate([0, 90, 0]) cylinder(d=1, h=150, center=true, $fn=4);
        translate([0, -pcb_rail_thickness/2, 0]) rotate([0, 90, 0]) cylinder(d=1, h=150, center=true, $fn=4);
        translate([136.6/2+0.75, pcb_rail_thickness/2, 0.8]) rotate([0, 0, 0]) cylinder(d=2, h=5, center=true, $fn=4);
        translate([136.6/2+0.75, -pcb_rail_thickness/2, 0.8]) rotate([0, 0, 0]) cylinder(d=2, h=5, center=true, $fn=4);
        translate([136.6/2+0.75, -pcb_rail_thickness/2, -1]) rotate([90, 0, 0]) rotate([0,0,-15]) cylinder(d=5, h=10, center=true, $fn=4);
    }

}


//pcb_edge();

module rail(){
    
    difference(){
        union(){
            translate([box_width/2-6, -albase_length/2, 0]) cube([6, albase_length, pcb_above_ground+pcb_rail_thickness+5]);
            
            translate([box_width/2-6, albase_length/2-15, 0]) cube([6, 15, pcb_above_ground+pcb_rail_thickness+5+10]);
            
            translate([box_width/2-flange_width, -(albase_length)/2, 0]) cube([flange_width, 60, flange_th]);
            translate([box_width/2-flange_width, -(albase_length)/2, 0]) cube([flange_width, 20, pcb_above_ground]);
            translate([box_width/2-8, -(albase_length)/2, 3.5]) rotate([0, 45, 0]) cube([4, albase_length, 4]);
            
            translate([box_width/2-10, -albase_length/2, 0]) cube([10, albase_length, flange_th]);
            
         
        
        }
        
        for(m=[0, 1]) mirror([0, m, 0]) {
            translate([pcb_width/2, -albase_length/2+15, 3]) rotate([90, 0, 0]) cylinder(d=M3_screw_diameter, h=20, $fn=60);
            
            hull(){
                translate([pcb_width/2, -albase_length/2+5, 3]) rotate([90, 0, 0]) cylinder(d=M3_nut_diameter, h=3, $fn=6);
                translate([pcb_width/2, -albase_length/2+5, -3]) rotate([90, 0, 0]) cylinder(d=M3_nut_diameter, h=3, $fn=6);
            }
        }
    
    translate([0, -(albase_length+1)/2, pcb_above_ground])
        cube([pcb_width/2, albase_length+1, pcb_thickness]);
    translate([0, -(albase_length-40)/2, pcb_above_ground-(pcb_rail_thickness-pcb_thickness)/2])
        cube([pcb_width/2+pcb_rail_depth, albase_length, pcb_rail_thickness]);
    
    hull(){
        translate([0, (albase_length)/2-10, pcb_above_ground-(pcb_rail_thickness-pcb_thickness)/2])
            cube([pcb_width/2+pcb_rail_depth, 10, pcb_rail_thickness]);
        translate([0, (albase_length)/2, pcb_above_ground-(pcb_rail_thickness-pcb_thickness)/2-2])
            cube([pcb_width/2+pcb_rail_depth+2, 10, pcb_rail_thickness+4]);
    }
    
    for(y = [5, 0], x=[10.16]) translate([mounting_holes_distance+x, (y+0.5)*10.16, -0.1]) {
        cylinder(d = M3_screw_diameter, h=5, $fn=30);
        translate([0, 0, 2]) rotate([0, 0, 30]) cylinder(d=7.5, h=7.5, $fn=60);
        }
        
    for(y = [-7], x=[10.16]) translate([mounting_holes_distance+x, (y+0.5)*10.16, -0.1]) {
        cylinder(d = M3_screw_diameter, h=20, $fn=30);
        translate([0, 0, pcb_above_ground+0.1]) cylinder(d = 6.5, h=20, $fn=30);
        }
    
    for(y = [-8+4], x=[0]) translate([mounting_holes_distance+x, (y+0.5)*10.16, -0.1]) {
        cylinder(d = M3_screw_diameter, h=30, $fn=30);
            translate([0, 0, 1.5]) rotate([0, 0, 30]) cylinder(d=M3_nut_diameter, h=3, $fn=6);
        }
        for(y = [-8], x=[0]) translate([mounting_holes_distance+x, (y+0.5)*10.16, -0.1]) {
        cylinder(d = M3_screw_diameter, h=30, $fn=30);
        //translate([0, 0, 3]) rotate([0, 0, 30]) cylinder(d=6.5, h=3, $fn=60);
        }
    }
    

}



//rail();
//mirror([1, 0, 0]) rail();


batdat_case_length = 136;
batdat_case_width = 82;
batdat_case_thickness = 7;

screw_shift = 12.0-10.16/2-4.83;

sn_view_position_x = 10.16*6-screw_shift+3.27;

module bottom_case(){

    difference(){
        translate([0, 0, batdat_case_thickness/2])
            cube([batdat_case_length, batdat_case_width, batdat_case_thickness], center=true);
    
        
        for(x=[-1, 1], y=[-1, 1])
            translate([x*10.16*6-screw_shift, y*10.16*3.5+0, -0.1]) {
            translate([0, 0, 3.2])
                cylinder(d=M3_screw_diameter, h=10, $fn=50);
            cylinder(d=6.6, h=3, $fn=6);
        }
        
        
        // pro senzory 
        translate([45, batdat_case_width/2, batdat_case_thickness]) cube([13, 10, 5], center=true);
            
        
        translate([0, 3, 0.4]) intersection(){
        for(x=[-8:7], y=[-4:3]) translate([x*7, y*8+(x%2==0? 4:0), 0]) cylinder(d=7.5, h=10, $fn=6);
        
        
        }
        
        
        
        // srazeni spodni hrany
        for(y=[-1, 1])
            translate([0, y/2*batdat_case_width, 0])
                rotate([0, 90, 0]) 
                    cylinder(h=batdat_case_length, r=2, center=true, $fn=4);

        // hlavni odebrani vyplne
        difference(){
            union(){
                translate([0, 0, 5+0.75]) 
                    cube([130, 77, 10], center=true);
                translate([-10, 0, 2.5+batdat_case_thickness-5]) 
                    cube([130, 62, 5], center=true);
                translate([-batdat_case_length/2, 0, 2.5+batdat_case_thickness-2.2]) 
                    cube([20, 100, 5], center=true);
            }
            
        
        // Okenko pro SN
        //translate([sn_view_position_x, 0, 0]) cube([8+1.4, 45+1.4, 20], center=true);
            
            
            
        for(x=[1], y=[-1, 1]) translate([x*10.16*6-screw_shift, y*10.16*3.5+0, -0.1]) {
            cylinder(d=8, h=10, $fn=60);
            cylinder(d1=12,d2=8, h=3, $fn=60);
        }
        
        
            
        for(x=[-1], y=[-1, 1]) translate([x*10.16*6-screw_shift, y*10.16*3.5+0, -0.1]) {
            cylinder(d=8, h=5+2.1-2.2, $fn=60);
            cylinder(d1=12,d2=8, h=3, $fn=60);
        }
            
        }
        
        // Okenko pro SN
        //translate([sn_view_position_x, 0, 0]) cube([8, 45, 20], center=true);
            
    
    
    }
    
    // Podperne sloupky
   for(y=[-31.6, -12.3, 9.4, 28.7, 48], x=[-0.5, 0.5]) translate([-y, x*55.6, 0]){
    
        cylinder(d = 6.5, h=2, $fn=30);
        cylinder(d = 4.5, h=batdat_case_thickness, $fn=30);
        cylinder(d = 3, h=batdat_case_thickness+3, $fn=30);
    }
    
    
}

//bottom_case();




box_outer_w = 103+0.5;
box_outer_h = 53+0.5;
box_pcb_shift = 10;


module front_cover_empty(){



    difference(){
        union(){
            minkowski(){
                sphere(d=10, $fn=8);
                translate([0, 0, 5]) cube([box_outer_w+5-10, box_outer_h+5-10, 4], center=true);
            }
    
        }
        
    
    translate([0, 0, 5+5]) cube([200, 200, 10], center=true);
        
    translate([0, 0, 1.25]) minkowski(){
        cylinder(d=8, h=10, $fn=30);
        cube([box_outer_w-8+0.6, box_outer_h-8+0.6, .1], center=true);
    }
    
    
    // Vyrez pro celou sirku ALBASE
    translate([0, -5, 0]) minkowski(){
        cylinder(d=8, h=10, $fn=30);
        cube([box_outer_w-8+0.6, 25-8+0.6, .1], center=true);
    }
    
    
    translate([0, 0, 0]) minkowski(){
        cylinder(d=2, h=10, $fn=30);
        cube([box_outer_w-8+0.6, box_outer_h-8+0.6, .1], center=true);
    }
    
       
    
    }
}


leds_positions_top = [25.07, 22.3, 19.52, 1.3, -1.46, -4.25, -7, -9.78, -12.53, -15.3, -18.1, -23.2, -27.1];
leds_positions_bottom = [-9.78, -15.3];
leds_button = [7.4, 14.75];

module front_cover(){

    difference(){

        union(){
            front_cover_empty();

            translate([0, 1.6/2, 0]) difference(){
                union(){
                    translate([0, 2.5+0.1, 3.5]) cube([80.7, 5, 9], center=true);
                    translate([0, -1-1.6-0.1, 3.5]) cube([80.7, 2, 9], center=true);
                }
                
                translate([0, -3, 2]) cube([31*2, 4, 15], center=true);
                difference(){
                    translate([0, 0.5, 2]) cube([31*2, 3, 15], center=true);
                    translate([10.5, 0.5, 2]) cube([14.5, 3, 15], center=true);
                }
                for(x=leds_button) translate([x, 1.75]) {
                    //cylinder(d=4,h=20, $fn=30, center=true);
                    translate([0, 0, 4]) cylinder(d=7,h=20, $fn=30);
                }   
                for(x=[0.5, -0.5])  translate([x*10.16*7, 0, 4.9]) rotate([90, 0, 0]) cylinder(d=M3_screw_diameter, h=15, center=true, $fn=30);
                

            }
        }
        
        for(x=leds_positions_top) translate([x, 1.6/2+0.6, 0]) scale([1, 1, 1]) cylinder(d1=1.6, d2=3,h=20, $fn=30, center=true);
        for(x=leds_positions_bottom) translate([x, -1.6/2-0.6, 0]) scale([1, 1, 1]) cylinder(d1=1.6, d2=3,h=20, $fn=30, center=true);
        for(x=leds_button) translate([x, 1.6/2+1.75]) cylinder(d=4,h=20, $fn=30, center=true);
     
  
   // USB-C konektor
    translate([-25.33, -pcb_thickness/2-2.6/2-0.25, 0]) minkowski(){
        cube([8.5-2, 2.6-2, 10], center=true);
        cylinder(d=2.3, h = 1, $fn=30);
    }   
    translate([-25.33, -pcb_thickness/2-2.6/2-0.25, 5-1.2]) minkowski(){
        cube([8.5-2, 2.6-2, 9], center=true);
        cylinder(d=3, h = 1, $fn=30);
    }   
    
    
    // Otvory pro stuhy
    for(x=[0.5, -0.5]) 
    translate([x*82, pcb_thickness/2+6/2, 0]) minkowski(){
        cube([0.1, 5.5-1, 10], center=true);
        cylinder(d=1, h = 1, $fn=30);
    }   
  
  
  // Otvory pro srouby na prisroubovani celicka
    translate([pcb_width/2, -pcb_above_ground+3/2+0.5, 0]) cylinder(d=M3_screw_diameter, h=10, center=true, $fn=60);
    translate([-pcb_width/2, -pcb_above_ground+3/2+0.5, 0]) cylinder(d=M3_screw_diameter, h=10, center=true, $fn=60);

    }

    /// lem na rozepreni proti PCB
    cube([60, 1, 0.8], center=true);
    
}

front_cover();


module button(){
    difference(){
        union(){
            cylinder(d=6.5, h=0.75+0.25, $fn=40);
            cylinder(d=4-0.2, h = 7+0.25, $fn=40);
        }
        translate([-5, -10-1.5, 0]) cube([10, 10, 7-2]);
        cylinder(d = 2.3, h = 0.4, $fn=40);
    }
};
//translate([7.5, 2.5, 15]) button();

module rear_cover(){

    difference(){
        union(){
            front_cover_empty();    
            //cube([, 5, 1.5]);
        
        }
        
        // Snizeni rantlu 
        translate([0, 0, 2.5+4]) cube([200, 100, 5], center=true);
        
        
        // Otvory pro srouby na prisroubovani celicka
        translate([-pcb_width/2, -pcb_above_ground+3/2+0.5, 0]) cylinder(d=M3_screw_diameter, h=10, center=true, $fn=60);
        translate([pcb_width/2, -pcb_above_ground+3/2+0.5, 0]) cylinder(d=M3_screw_diameter, h=10, center=true, $fn=60);
        
        for(x=[0.5, -0.5], y=[-0.5, 0.5]) translate([x*95, y*45.47, -1.7]) {
            cylinder(d=4.2, h = 10, $fn=60);
            cylinder(d1=5.85, d2=4.2, h = 1, $fn=60);
        }

        // vybrani pro zabraneni kontaktu USTSIPIN03 se stenou cela
        translate([0, -21.3, 5/2-0.6]) cube([80, 5, 5], center=true);
    }
    
    translate([0, 1.6/2, 0]) difference(){
        translate([0, 3.5, 4]) cube([70.6, 7, 10], center=true);
        translate([0, 2.5, 4]) cube([70.6-10.16*2, 10, 10+1], center=true);
       
        for(x=[0.5, -0.5])  translate([x*10.16*6, 0, 4.9]) rotate([90, 0, 0]) {
            cylinder(d=M3_screw_diameter, h=10, center=true, $fn=30);
            translate([0, 0, -10-7+3]) cylinder(d=M3_nut_diameter, h=10, center=false, $fn=6);
       
        }
        
    }
}

//rear_cover();