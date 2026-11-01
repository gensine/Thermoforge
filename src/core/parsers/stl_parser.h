#ifndef STL_PARSER_H
#define STL_PARSER_H

#include <string>
#include <vector>

namespace workbench::parsers {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct STLTriangle {
    Vec3 normal;
    Vec3 v1;
    Vec3 v2;
    Vec3 v3;
};

struct BoundingBox {
    Vec3 min{1e30, 1e30, 1e30};
    Vec3 max{-1e30, -1e30, -1e30};
    Vec3 center{0, 0, 0};
    double diagonal = 0.0;
};

struct STLModel {
    std::string solid_name;
    std::vector<STLTriangle> triangles;
    BoundingBox bbox;
    size_t facet_count = 0;
    bool is_valid = false;
    std::string error_message;
};

class STLParser {
public:
    // Parses and validates an ASCII STL file from disk
    static STLModel parse_file(const std::string& filepath);

    // Parses and validates an ASCII STL from string content
    static STLModel parse_string(const std::string& content);

    // Exports the model to a compact JSON string for frontend rendering
    static std::string to_json(const STLModel& model);
};

} // namespace workbench::parsers

#endif // STL_PARSER_H
