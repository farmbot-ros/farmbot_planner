#include <cmath>
#include <iostream>

#include "rclcpp/rclcpp.hpp"

#include "fields2cover.h"
// #include "farmbot_planner/utils/geojson_parser.hpp"

int run() {
  // Import field
    F2CField field = f2c::Parser::importFieldGml("/home/bresilla/FARMBOT/src/farmbot_planner/config/test1.xml");
    F2CField orig_field = field.clone();
//   // Transform into UTM to work in meters
    // f2c::Transform::transformToUTM(field);
    // F2CCells cells = field.getField();

    // auto field_parsed = geojson::parseFile("/home/bresilla/FARMBOT/src/farmbot_planner/config/field.geojson");

  // F2CCells cells(F2CCell(F2CLinearRing({
  //       F2CPoint(  0,  0),
  //       F2CPoint( 90,  0),
  //       F2CPoint( 90, 20),
  //       F2CPoint( 20, 20),
  //       F2CPoint( 20, 90),
  //       F2CPoint(  0, 90),
  //       F2CPoint(  0,  0)})));

  // F2CRobot robot (0.5, 1);
  // f2c::hg::ConstHL const_hl;
  // F2CCells no_hl = const_hl.generateHeadlands(cells, 3.0 * robot.getWidth());
  // f2c::sg::BruteForce bf;
  // F2CSwaths swaths = bf.generateSwaths(M_PI/1.32, robot.getCovWidth(), no_hl.getGeometry(0));
  // f2c::rp::SnakeOrder snake_sorter;
  // swaths = snake_sorter.genSortedSwaths(swaths);
  // f2c::pp::PathPlanning path_planner;
  // robot.setMinTurningRadius(2);  // m
  // f2c::pp::DubinsCurves dubins;
  // F2CPath path = path_planner.planPath(robot, swaths, dubins);


  // f2c::Visualizer::figure();
  // f2c::Visualizer::plot(cells);
  // f2c::Visualizer::plot(no_hl);
  // f2c::Visualizer::plot(path);
  // f2c::Visualizer::save("Tutorial_8_1_UTM.png");


//   // Transform the generated path back to the previousa CRS.
//   F2CPath path_gps = f2c::Transform::transformToPrevCRS(path, field);
//   f2c::Transform::transformToPrevCRS(field);

//   f2c::Visualizer::figure();
//   f2c::Visualizer::plot(orig_field.getCellsAbsPosition());
//   f2c::Visualizer::plot(path_gps);
//   f2c::Visualizer::save("Tutorial_8_1_GPS.png");

  return 0;
}


class FieldsGen : public rclcpp::Node {
    private:
        rclcpp::TimerBase::SharedPtr fields_gen_timer_;
    public:
        FieldsGen() : Node("fields_gen") {
            RCLCPP_INFO(this->get_logger(), "Starting FieldsGen node");
            fields_gen_timer_ = this->create_wall_timer(std::chrono::milliseconds(10000), std::bind(&FieldsGen::fields_gen_timer, this));
        }
    private:
        void fields_gen_timer() {
            run();
        }
};



int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    auto navfix = std::make_shared<FieldsGen>();
    rclcpp::spin(navfix);
    rclcpp::shutdown();
    return 0;
}