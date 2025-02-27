use<pcb_rails.scad>;


difference(){
front_cover();

translate([-150/2,-150-5,-10])
            cube([150, 150, 20], center=false);
}
