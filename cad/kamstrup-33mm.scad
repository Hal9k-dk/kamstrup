$fa = 1;

module magnethole()
{
  translate([0,0,-0.1]) cylinder(r=3, h=6.1, $fn=20);
}
//magnethole();


module ledhole()
{
  translate([0,0,-0.1]) cylinder(r=2.5, h=20, $fn=20);
  translate([0,0,8]) cylinder(r=3, h=20, $fn=20);
}
//ledhole();

module outer()
{
  difference() {
	cylinder(r=16.5, h=25);
      translate([0, 0, 10]) cylinder(r=14.5, h=25);
//      translate([0, 0, -1]) cylinder(r=6.5, h=5);
  }
}

module lid()
{
  union()
 {
    cylinder(r=16.5, h=2.8);
    translate([0,0,2.8]) cylinder(r=14.25, h=2.8);
  }
}

//lid();

union() {
difference() {
  outer();
  rotate(45) translate([11,0,0]) magnethole();
  rotate(135) translate([11,0,0]) magnethole();
  rotate(-45) translate([11,0,0]) magnethole();
  rotate(-135) translate([11,0,0]) magnethole();

  translate([3.5,0,0]) ledhole();
  translate([-3.5,0,0]) ledhole();
  translate([0,0,18]) rotate([90,90]) cylinder(r=2, h=20, $fn=20);
}


  translate([40,0,0]) lid();

}