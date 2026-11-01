#include "ansys_csv_parser.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <iomanip>

namespace workbench::parsers {

static inline std::string trim_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static inline std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream stream(line);
    while (std::getline(stream, token, ',')) {
        tokens.push_back(trim_str(token));
    }
    return tokens;
}

ANSYSModel ANSYSCSVParser::parse_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        ANSYSModel model;
        model.is_valid = false;
        model.error_message = "Cannot open CSV file: '" + filepath + "'. Check file path and access permissions.";
        return model;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    if (content.empty()) {
        ANSYSModel model;
        model.is_valid = false;
        model.error_message = "The ANSYS CSV file is completely empty (0 bytes). Please supply valid nodal export data.";
        return model;
    }

    return parse_string(content);
}

ANSYSModel ANSYSCSVParser::parse_string(const std::string& content) {
    ANSYSModel model;
    std::string trimmed_all = trim_str(content);
    if (trimmed_all.empty()) {
        model.is_valid = false;
        model.error_message = "File contains only whitespace or blank lines.";
        return model;
    }

    std::istringstream stream(content);
    std::string line;
    size_t line_number = 0;

    // Read header row
    bool header_found = false;
    int col_node = -1, col_x = -1, col_y = -1, col_z = -1, col_temp = -1;

    while (std::getline(stream, line)) {
        line_number++;
        std::string tline = trim_str(line);
        if (tline.empty()) continue;

        // Check if comment or metadata header
        if (tline[0] == '#') {
            if (tline.find("SYNTHETIC") != std::string::npos || tline.find("synthetic") != std::string::npos) {
                model.provenance_note = "Synthetic Dataset (Self-created for verification and testing)";
            }
            continue;
        }

        auto headers = split_csv_line(tline);
        if (headers.size() < 5) {
            model.is_valid = false;
            model.error_message = "Header line " + std::to_string(line_number) + 
                                  " has fewer than 5 columns. Expected: node_id,x_m,y_m,z_m,temperature_C";
            return model;
        }

        for (size_t c = 0; c < headers.size(); ++c) {
            std::string h = headers[c];
            std::transform(h.begin(), h.end(), h.begin(), ::tolower);
            if (h == "node_id" || h == "node" || h == "id") col_node = static_cast<int>(c);
            else if (h == "x_m" || h == "x") col_x = static_cast<int>(c);
            else if (h == "y_m" || h == "y") col_y = static_cast<int>(c);
            else if (h == "z_m" || h == "z") col_z = static_cast<int>(c);
            else if (h == "temperature_c" || h == "temperature" || h == "temp_c" || h == "temp") col_temp = static_cast<int>(c);
        }

        if (col_node == -1 || col_x == -1 || col_y == -1 || col_z == -1 || col_temp == -1) {
            model.is_valid = false;
            model.error_message = "Missing required column headers on line " + std::to_string(line_number) + 
                                  ". Required: 'node_id', 'x_m', 'y_m', 'z_m', 'temperature_C'.";
            return model;
        }

        header_found = true;
        break;
    }

    if (!header_found) {
        model.is_valid = false;
        model.error_message = "CSV parsing failed: No valid column header was detected.";
        return model;
    }

    // Parse data rows
    double sum_temp = 0.0;
    while (std::getline(stream, line)) {
        line_number++;
        std::string tline = trim_str(line);
        if (tline.empty()) continue;
        if (tline[0] == '#') continue; // comment line

        auto tokens = split_csv_line(tline);
        size_t max_req_col = std::max({col_node, col_x, col_y, col_z, col_temp});
        if (tokens.size() <= max_req_col) {
            model.is_valid = false;
            model.error_message = "Malformed row at line " + std::to_string(line_number) + 
                                  ": Expected at least " + std::to_string(max_req_col + 1) + 
                                  " fields, found " + std::to_string(tokens.size());
            return model;
        }

        ANSYSNode node;

        // Parse node_id
        try {
            node.node_id = std::stoll(tokens[col_node]);
        } catch (...) {
            model.is_valid = false;
            model.error_message = "Data format error at line " + std::to_string(line_number) + 
                                  ": Invalid node_id integer '" + tokens[col_node] + "'.";
            return model;
        }

        // Parse coordinates and temperature
        try {
            node.x = std::stod(tokens[col_x]);
            node.y = std::stod(tokens[col_y]);
            node.z = std::stod(tokens[col_z]);
            node.temperature_C = std::stod(tokens[col_temp]);
        } catch (...) {
            model.is_valid = false;
            model.error_message = "Data format error at line " + std::to_string(line_number) + 
                                  ": Non-numeric floating point value encountered.";
            return model;
        }

        // Validate finiteness (NaN or Inf)
        if (!std::isfinite(node.x) || !std::isfinite(node.y) || !std::isfinite(node.z)) {
            model.is_valid = false;
            model.error_message = "Data validation error at line " + std::to_string(line_number) + 
                                  ": Node coordinates contain NaN or Inf.";
            return model;
        }
        if (!std::isfinite(node.temperature_C)) {
            model.is_valid = false;
            model.error_message = "Data validation error at line " + std::to_string(line_number) + 
                                  ": Temperature contains non-finite value (NaN or Inf).";
            return model;
        }

        // Update bounding box
        model.bbox.min.x = std::min(model.bbox.min.x, node.x);
        model.bbox.min.y = std::min(model.bbox.min.y, node.y);
        model.bbox.min.z = std::min(model.bbox.min.z, node.z);

        model.bbox.max.x = std::max(model.bbox.max.x, node.x);
        model.bbox.max.y = std::max(model.bbox.max.y, node.y);
        model.bbox.max.z = std::max(model.bbox.max.z, node.z);

        // Update temperature range
        model.min_temp_C = std::min(model.min_temp_C, node.temperature_C);
        model.max_temp_C = std::max(model.max_temp_C, node.temperature_C);
        sum_temp += node.temperature_C;

        model.nodes.push_back(node);
    }

    if (model.nodes.empty()) {
        model.is_valid = false;
        model.error_message = "CSV contains valid headers but zero data rows.";
        return model;
    }

    model.node_count = model.nodes.size();
    model.mean_temp_C = sum_temp / static_cast<double>(model.node_count);

    // Finalize bounding box center and diagonal
    model.bbox.center.x = (model.bbox.min.x + model.bbox.max.x) * 0.5;
    model.bbox.center.y = (model.bbox.min.y + model.bbox.max.y) * 0.5;
    model.bbox.center.z = (model.bbox.min.z + model.bbox.max.z) * 0.5;

    double dx = model.bbox.max.x - model.bbox.min.x;
    double dy = model.bbox.max.y - model.bbox.min.y;
    double dz = model.bbox.max.z - model.bbox.min.z;
    model.bbox.diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (model.provenance_note.empty()) {
        model.provenance_note = "ANSYS Workbench FEA Export (Nodal thermal post-processing)";
    }

    model.is_valid = true;
    return model;
}

std::string ANSYSCSVParser::generate_synthetic_dataset(int grid_x, int grid_y, int grid_z) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    oss << "# PROVENANCE: SYNTHETIC DATASET (Self-created for Unified Engineering Workbench validation)\n";
    oss << "# Schema: node_id (int), x_m (meters), y_m (meters), z_m (meters), temperature_C (Celsius)\n";
    oss << "# Units: [x,y,z]=meters, [temperature]=deg C\n";
    oss << "node_id,x_m,y_m,z_m,temperature_C\n";

    int64_t id = 1;
    double Lx = 0.2; // 200 mm block
    double Ly = 0.1; // 100 mm
    double Lz = 0.05; // 50 mm

    for (int i = 0; i < grid_x; ++i) {
        double x = (grid_x > 1) ? (i * Lx / (grid_x - 1)) : 0.0;
        for (int j = 0; j < grid_y; ++j) {
            double y = (grid_y > 1) ? (j * Ly / (grid_y - 1)) : 0.0;
            for (int k = 0; k < grid_z; ++k) {
                double z = (grid_z > 1) ? (k * Lz / (grid_z - 1)) : 0.0;

                // Thermal diffusion field with localized heat source at center
                double dist_center = std::sqrt(std::pow(x - Lx*0.5, 2) + std::pow(y - Ly*0.5, 2) + std::pow(z - Lz*0.5, 2));
                double temp = 25.0 + 95.0 * std::exp(-dist_center / 0.04) + 15.0 * std::sin(3.14159 * x / Lx);

                oss << id++ << "," << x << "," << y << "," << z << "," << temp << "\n";
            }
        }
    }

    return oss.str();
}

std::string ANSYSCSVParser::to_json(const ANSYSModel& m) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    oss << "{";
    oss << "\"valid\":" << (m.is_valid ? "true" : "false") << ",";
    oss << "\"node_count\":" << m.node_count << ",";
    oss << "\"error_message\":\"" << m.error_message << "\",";
    oss << "\"provenance\":\"" << m.provenance_note << "\",";
    oss << "\"min_temp_C\":" << m.min_temp_C << ",";
    oss << "\"max_temp_C\":" << m.max_temp_C << ",";
    oss << "\"mean_temp_C\":" << m.mean_temp_C << ",";
    oss << "\"bbox\":{";
    oss << "\"min\":[" << m.bbox.min.x << "," << m.bbox.min.y << "," << m.bbox.min.z << "],";
    oss << "\"max\":[" << m.bbox.max.x << "," << m.bbox.max.y << "," << m.bbox.max.z << "],";
    oss << "\"center\":[" << m.bbox.center.x << "," << m.bbox.center.y << "," << m.bbox.center.z << "],";
    oss << "\"diagonal\":" << m.bbox.diagonal;
    oss << "},";

    // Flattened positions and temperatures for WebGL point cloud buffers
    oss << "\"positions\":[";
    for (size_t i = 0; i < m.nodes.size(); ++i) {
        if (i > 0) oss << ",";
        oss << m.nodes[i].x << "," << m.nodes[i].y << "," << m.nodes[i].z;
    }
    oss << "],";

    oss << "\"temperatures\":[";
    for (size_t i = 0; i < m.nodes.size(); ++i) {
        if (i > 0) oss << ",";
        oss << m.nodes[i].temperature_C;
    }
    oss << "],";

    oss << "\"node_ids\":[";
    for (size_t i = 0; i < m.nodes.size(); ++i) {
        if (i > 0) oss << ",";
        oss << m.nodes[i].node_id;
    }
    oss << "]";
    oss << "}";

    return oss.str();
}

} // namespace workbench::parsers
