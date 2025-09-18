

module battery_case(){

battery_length = 101;
battery_width = 79;
battery_height = 22;
wall_around = 1;
wall_top = 1.5;


beam_length = 130;
beam_thickness = 10;
front_offset = 6;

screw_distance = 10.16*12;


difference(){
    translate([0, 0, battery_height/2-wall_top/2]) cube([battery_length+wall_around*2, battery_width+wall_around*2, battery_height+wall_top], center=true);
    translate([0, 0, battery_height/2]) cube([battery_length, battery_width, battery_height], center=true);
}


offset = 5;

for(y=[-0.5, 0.5]) translate([0, y*10.16*7, 0]) difference(){
hull(){
    translate([offset, 0, battery_height-3]) cube([beam_length, beam_thickness, 6], center=true);
    translate([3, 0, 0.05]) cube([battery_length+2*wall_around+5+3, beam_thickness, 0.1+wall_top*2], center=true);

}
    translate([0, 0, battery_height/2]) cube([battery_length, battery_width, battery_height], center=true);
    
    translate([-20, 0, battery_height*1.5-5]) cube([battery_length, battery_width, battery_height], center=true);

translate([battery_length/2+wall_around+0.5, 0, battery_height-4])
    cube([1, 100, 6], center=true);

for(x=[0.5, -0.5]) translate([offset+screw_distance*x, 0, 0]){
    cylinder(d=3.3, h=50, center=true, $fn=50);
    cylinder(d=6.5, h=30, center=true, $fn=6);

}
}


}



battery_case();