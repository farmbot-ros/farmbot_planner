#ifndef SWATH_HPP
#define SWATH_HPP

#include "field.hpp"
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/segment.hpp>
#include <boost/geometry/algorithms/intersection.hpp>
#include <boost/geometry/algorithms/envelope.hpp>
#include <boost/geometry/algorithms/expand.hpp>
#include <boost/geometry/algorithms/distance.hpp> // For calculating distance

// Include Boost libraries for graph and R-tree
#include <boost/geometry/index/rtree.hpp>
#include <boost/graph/prim_minimum_spanning_tree.hpp>

#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.

#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <queue>
#include <utility> // For std::pair

namespace farmtrax {
    namespace bg = boost::geometry;
    namespace bgi = boost::geometry::index;

    // Define Cartesian point type
    typedef bg::model::d2::point_xy<double> Point;
    // Define polygon type using the Cartesian point
    typedef bg::model::polygon<Point> Polygon;
    // Define linestring type for edges
    typedef bg::model::linestring<Point> LineString;
    // Define box type
    typedef bg::model::box<Point> Box;
    // Define a multi-polygon type
    typedef bg::model::multi_polygon<Polygon> Multipolygon;

    // Enum class to represent different types of swaths
    enum class SwathType {
        LAND,
        HEAD,
        PATH
    };

    enum class Direction {
        FORWARD,
        REVERSE
    };

    // Struct to represent each swath, along with its properties
    struct Swath {
        LineString swath;         // The actual swath line (geometry)
        std::string uuid;         // A unique identifier for each swath
        SwathType type;           // The type of swath (LAND, HEAD, PATH)
        Direction direction;      // The direction of the swath (FORWARD, REVERSE)
        bool transportlane;       // Flag to indicate if it's a transport lane
        float length;             // Length of the swath
    };

    // R-tree type definitions
    typedef std::pair<Box, std::size_t> RtreeValue;
    typedef bgi::rtree<RtreeValue, bgi::quadratic<16>> Rtree;

    class Swaths {
    private:
        Field inner_field_;
        Field outer_field_;
        double swath_width_;
        double angle_degrees_;
        std::vector<Swath> swaths_;  // Holds Swath structs
        Rtree swath_rtree_;          // R-tree for efficient spatial querying of swaths

    public:
        Swaths() = default;

        // Constructor to initialize with a field and swath width
        Swaths(const Field& outer_field, const Field& inner_field, double swath_width, double angle_degrees) {
            gen_swaths(outer_field, inner_field, swath_width, angle_degrees);
        }

        void gen_swaths(const Field& outer_field, const Field& inner_field, double swath_width, double angle_degrees) {
            outer_field_ = outer_field;
            inner_field_ = inner_field;
            swath_width_ = swath_width;
            angle_degrees_ = angle_degrees;
            generate_swaths();
        }

        // Get the swaths as a vector of Swath structs
        std::vector<Swath> get_swaths() const {
            return swaths_;
        }

        // Query swaths intersecting a given bounding box
        std::vector<std::size_t> query_swaths(const Box& query_box) const {
            std::vector<RtreeValue> result_s;
            swath_rtree_.query(bgi::intersects(query_box), std::back_inserter(result_s));

            std::vector<std::size_t> swath_indices;
            swath_indices.reserve(result_s.size());
            for (const auto& val : result_s) {
                swath_indices.push_back(val.second);
            }
            return swath_indices;
        }

        // Find the nearest swath to a given point
        std::size_t nearest_swath(const Point& point) const {
            std::vector<RtreeValue> result_s;
            swath_rtree_.query(bgi::nearest(point, 1), std::back_inserter(result_s));

            if (!result_s.empty()) {
                return result_s.front().second;
            } else {
                throw std::runtime_error("No swaths available.");
            }
        }
        
        //check if two points are connected by a swath
        bool are_connected(const Point& p1, const Point& p2) {
            LineString connection = create_connection(p1, p2);
            for (const auto& swath : swaths_) {
                if (bg::intersects(connection, swath.swath)) {
                    return true;
                }
            }
            return false;
        }

    private:
        // Helper function to generate swaths with a specified angle
        void generate_swaths(int alternate_freq = 1) {
            swaths_.clear();
            swath_rtree_.clear(); // Clear existing entries

            Field new_field = inner_field_.get_buffered(0.5, farmtrax::BufferType::ENLARGE);
            Polygon fieldPolygon = new_field.get_polygon();

            // Get the bounding box of the field
            boost::geometry::model::box<Point> boundingBox;
            boost::geometry::envelope(fieldPolygon, boundingBox);

            // Convert angle from degrees to radians
            double angle_radians = angle_degrees_ * M_PI / 180.0;

            // Determine the dimensions of the bounding box
            double width = boundingBox.max_corner().x() - boundingBox.min_corner().x();
            double height = boundingBox.max_corner().y() - boundingBox.min_corner().y();
            double max_dim = std::max(width, height);

            // Starting point and ending point adjusted for angle
            Point centerPoint((boundingBox.min_corner().x() + boundingBox.max_corner().x()) / 2,
                              (boundingBox.min_corner().y() + boundingBox.max_corner().y()) / 2);

            // Iterate to generate swaths with a defined offset based on swath width
            bool alternate = true;
            int counter = 0;
            for (double offset = -max_dim / 2; offset <= max_dim / 2; offset += swath_width_) {
                counter++;
                // Alternate the direction of the swaths based on the frequency
                alternate = (counter % alternate_freq == 0) ? !alternate : alternate;
                LineString swathLine = generate_swathine(centerPoint, angle_radians, offset);
                // Clip the swath line to fit within the field polygon

                std::vector<LineString> clipped;
                boost::geometry::intersection(swathLine, fieldPolygon, clipped);
                // Keep all valid segments of the swath that intersect the field polygon

                for (const auto& segment : clipped) {
                    Swath swath;  // Create a Swath struct for each segment
                    swath.swath = segment;
                    swath.uuid = generate_UUID();  // Generate a unique ID for each swath
                    swath.type = SwathType::LAND; // Always mark as LAND here
                    swath.transportlane = false;  // Default, can modify as needed
                    swath.length = bg::length(segment); // Calculate the length of the swath
                    swath.direction = alternate ? Direction::FORWARD : Direction::REVERSE;
                    swaths_.push_back(swath);

                    // Insert the swath into the R-tree
                    Box swath_box;
                    boost::geometry::envelope(segment, swath_box);
                    swath_rtree_.insert(std::make_pair(swath_box, swaths_.size() - 1));
                }
            }       
        }

        // Function to generate a line at a certain offset from the center, adjusted for the angle
        LineString generate_swathine(const Point& center, double angle_radians, double offset) const {
            LineString swathLine;

            // Calculate the perpendicular offset direction based on the angle
            double cos_angle = std::cos(angle_radians);
            double sin_angle = std::sin(angle_radians);

            // Define the half-length of the swath lines (to extend equally from the center)
            double half_length = std::max(outer_field_.get_width(), outer_field_.get_height());

            // Calculate the start and end points of the swath line
            Point newStart(center.x() + (offset * sin_angle) - (half_length * cos_angle),
                          center.y() - (offset * cos_angle) - (half_length * sin_angle));

            Point newEnd(center.x() + (offset * sin_angle) + (half_length * cos_angle),
                        center.y() - (offset * cos_angle) + (half_length * sin_angle));

            // Add the new start and end points to the swath line
            swathLine.push_back(newStart);
            swathLine.push_back(newEnd);

            return swathLine;
        }

        // Function to create a connection between two points
        LineString create_connection(const Point& p1, const Point& p2) const {
            LineString connection;
            connection.push_back(p1);
            connection.push_back(p2);
            return connection;
        }

        // Function to add a connecting swath to the swath list
        void add_connecting_swath(const LineString& connection) {
            Swath connecting_swath;
            connecting_swath.swath = connection;
            connecting_swath.uuid = generate_UUID();
            connecting_swath.type = SwathType::HEAD;  // Only connections are labeled as HEAD
            connecting_swath.transportlane = false;   // Can be modified as needed
            swaths_.push_back(connecting_swath);

            // Insert the connecting swath into the R-tree
            Box swath_box;
            boost::geometry::envelope(connection, swath_box);
            swath_rtree_.insert(std::make_pair(swath_box, swaths_.size() - 1));
        }

        // Function to generate a unique identifier for each swath
        std::string generate_UUID() const {
            return boost::uuids::to_string(boost::uuids::random_generator()());
        }

        //function that cheks if swath touches perimeter of the field
        bool intersects_field(const LineString& swath, const Field& field) {
            auto edges = field.get_edges();
            for (const auto& edge : edges) {
                if (bg::intersects(swath, edge)) {
                    return true;
                }
            }
            return false;
        }
    };

} // namespace farmtrax

#endif // SWATH_HPP
