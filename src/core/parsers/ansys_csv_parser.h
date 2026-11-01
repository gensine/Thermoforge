#ifndef ANSYS_CSV_PARSER_H
#define ANSYS_CSV_PARSER_H

#include <string>
#include <vector>
#include <cstdint>
#include "../parsers/stl_parser.h"

namespace workbench::parsers {

struct ANSYSNode {
    int64_t node_id = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double temperature_C = 0.0;
};

struct ANSYSModel {
    std::vector<ANSYSNode> nodes;
    BoundingBox bbox;
    double min_temp_C = 1e30;
    double max_temp_C = -1e30;
    double mean_temp_C = 0.0;
    size_t node_count = 0;
    bool is_valid = false;
    std::string error_message;
    std::string provenance_note;
};

class ANSYSCSVParser {
public:
    // Parses and validates an ANSYS temperature nodal results CSV from file
    static ANSYSModel parse_file(const std::string& filepath);

    // Parses and validates from string content
    static ANSYSModel parse_string(const std::string& content);

    // Generates a synthetic benchmark ANSYS CSV (clearly labeled synthetic)
    static std::string generate_synthetic_dataset(int grid_x = 10, int grid_y = 10, int grid_z = 10);

    // Serializes to JSON for WebGL point cloud rendering and node inspection
    static std::string to_json(const ANSYSModel& model);
};

} // namespace workbench::parsers

#endif // ANSYS_CSV_PARSER_H
