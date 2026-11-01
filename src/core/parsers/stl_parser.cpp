#include "stl_parser.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <iomanip>

namespace workbench::parsers {

static inline bool is_finite_vec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

static inline void update_bbox(BoundingBox& box, const Vec3& p) {
    box.min.x = std::min(box.min.x, p.x);
    box.min.y = std::min(box.min.y, p.y);
    box.min.z = std::min(box.min.z, p.z);

    box.max.x = std::max(box.max.x, p.x);
    box.max.y = std::max(box.max.y, p.y);
    box.max.z = std::max(box.max.z, p.z);
}

static inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

STLModel STLParser::parse_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        STLModel model;
        model.is_valid = false;
        model.error_message = "Cannot open file: '" + filepath + "'. Check path and permissions.";
        return model;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    if (content.empty()) {
        STLModel model;
        model.is_valid = false;
        model.error_message = "The STL file is completely empty (0 bytes).";
        return model;
    }

    return parse_string(content);
}

STLModel STLParser::parse_string(const std::string& content) {
    STLModel model;
    std::istringstream stream(content);
    std::string line;

    // Check for empty or whitespace-only content
    std::string trimmed_all = trim(content);
    if (trimmed_all.empty()) {
        model.is_valid = false;
        model.error_message = "File contains only whitespace characters.";
        return model;
    }

    // Read first non-empty line
    bool found_solid = false;
    size_t line_number = 0;
    while (std::getline(stream, line)) {
        line_number++;
        std::string tline = trim(line);
        if (tline.empty()) continue;

        std::istringstream iss(tline);
        std::string token;
        iss >> token;
        std::transform(token.begin(), token.end(), token.begin(), ::tolower);

        if (token == "solid") {
            found_solid = true;
            std::string name;
            std::getline(iss, name);
            model.solid_name = trim(name);
            break;
        } else {
            model.is_valid = false;
            model.error_message = "Malformed ASCII STL: Expected 'solid' header on line " + 
                                  std::to_string(line_number) + ", got '" + token + "'.";
            return model;
        }
    }

    if (!found_solid) {
        model.is_valid = false;
        model.error_message = "Malformed ASCII STL: File did not contain a valid 'solid' statement.";
        return model;
    }

    STLTriangle current_triangle;
    int vertex_count = 0;
    bool in_facet = false;
    bool in_loop = false;

    while (std::getline(stream, line)) {
        line_number++;
        std::string tline = trim(line);
        if (tline.empty()) continue;

        std::istringstream iss(tline);
        std::string token;
        iss >> token;
        std::string lower_token = token;
        std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(), ::tolower);

        if (lower_token == "facet") {
            if (in_facet) {
                model.is_valid = false;
                model.error_message = "Parse error line " + std::to_string(line_number) + 
                                      ": Nested 'facet' definition is invalid.";
                return model;
            }
            std::string normal_token;
            iss >> normal_token;
            std::transform(normal_token.begin(), normal_token.end(), normal_token.begin(), ::tolower);
            if (normal_token != "normal") {
                model.is_valid = false;
                model.error_message = "Parse error line " + std::to_string(line_number) + 
                                      ": Expected 'normal' after 'facet'.";
                return model;
            }
            if (!(iss >> current_triangle.normal.x >> current_triangle.normal.y >> current_triangle.normal.z)) {
                model.is_valid = false;
                model.error_message = "Parse error line " + std::to_string(line_number) + 
                                      ": Failed to parse normal components (x, y, z).";
                return model;
            }
            if (!is_finite_vec(current_triangle.normal)) {
                model.is_valid = false;
                model.error_message = "Data validation error line " + std::to_string(line_number) + 
                                      ": Normal contains non-finite value (NaN or Inf).";
                return model;
            }
            in_facet = true;
            vertex_count = 0;
        }
        else if (lower_token == "outer") {
            std::string loop_token;
            iss >> loop_token;
            std::transform(loop_token.begin(), loop_token.end(), loop_token.begin(), ::tolower);
            if (loop_token != "loop") {
                model.is_valid = false;
                model.error_message = "Parse error line " + std::to_string(line_number) + 
                                      ": Expected 'loop' after 'outer'.";
                return model;
            }
            in_loop = true;
        }
        else if (lower_token == "vertex") {
            if (!in_loop) {
                model.is_valid = false;
                model.error_message = "Parse error line " + std::to_string(line_number) + 
                                      ": 'vertex' encountered outside of 'outer loop'.";
                return model;
            }
            Vec3 v;
            if (!(iss >> v.x >> v.y >> v.z)) {
                model.is_valid = false;
                model.error_message = "Parse error line " + std::to_string(line_number) + 
                                      ": Invalid coordinates for vertex " + std::to_string(vertex_count + 1);
                return model;
            }
            if (!is_finite_vec(v)) {
                model.is_valid = false;
                model.error_message = "Data validation error line " + std::to_string(line_number) + 
                                      ": Vertex coordinates contain non-finite value (NaN or Inf).";
                return model;
            }
            if (vertex_count == 0) current_triangle.v1 = v;
            else if (vertex_count == 1) current_triangle.v2 = v;
            else if (vertex_count == 2) current_triangle.v3 = v;
            else {
                model.is_valid = false;
                model.error_message = "Parse error line " + std::to_string(line_number) + 
                                      ": Facet contains more than 3 vertices (polygons not supported in STL standard).";
                return model;
            }
            vertex_count++;
        }
        else if (lower_token == "endloop") {
            if (!in_loop || vertex_count != 3) {
                model.is_valid = false;
                model.error_message = "Parse error line " + std::to_string(line_number) + 
                                      ": 'endloop' closed facet with " + std::to_string(vertex_count) + 
                                      " vertices (exactly 3 required).";
                return model;
            }
            in_loop = false;
        }
        else if (lower_token == "endfacet") {
            if (!in_facet) {
                model.is_valid = false;
                model.error_message = "Parse error line " + std::to_string(line_number) + 
                                      ": 'endfacet' without corresponding 'facet'.";
                return model;
            }
            // Update bounding box
            update_bbox(model.bbox, current_triangle.v1);
            update_bbox(model.bbox, current_triangle.v2);
            update_bbox(model.bbox, current_triangle.v3);

            model.triangles.push_back(current_triangle);
            in_facet = false;
        }
        else if (lower_token == "endsolid") {
            break;
        }
    }

    if (model.triangles.empty()) {
        model.is_valid = false;
        model.error_message = "STL file parsed successfully but contains 0 facets.";
        return model;
    }

    // Finalize bounding box center and diagonal
    model.bbox.center.x = (model.bbox.min.x + model.bbox.max.x) * 0.5;
    model.bbox.center.y = (model.bbox.min.y + model.bbox.max.y) * 0.5;
    model.bbox.center.z = (model.bbox.min.z + model.bbox.max.z) * 0.5;

    double dx = model.bbox.max.x - model.bbox.min.x;
    double dy = model.bbox.max.y - model.bbox.min.y;
    double dz = model.bbox.max.z - model.bbox.min.z;
    model.bbox.diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);

    model.facet_count = model.triangles.size();
    model.is_valid = true;
    return model;
}

std::string STLParser::to_json(const STLModel& m) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    oss << "{";
    oss << "\"valid\":" << (m.is_valid ? "true" : "false") << ",";
    oss << "\"solid_name\":\"" << m.solid_name << "\",";
    oss << "\"facet_count\":" << m.facet_count << ",";
    oss << "\"error_message\":\"" << m.error_message << "\",";
    oss << "\"bbox\":{";
    oss << "\"min\":[" << m.bbox.min.x << "," << m.bbox.min.y << "," << m.bbox.min.z << "],";
    oss << "\"max\":[" << m.bbox.max.x << "," << m.bbox.max.y << "," << m.bbox.max.z << "],";
    oss << "\"center\":[" << m.bbox.center.x << "," << m.bbox.center.y << "," << m.bbox.center.z << "],";
    oss << "\"diagonal\":" << m.bbox.diagonal;
    oss << "},";

    // Serialize vertices and normals as flat arrays for WebGL buffer consumption
    // 3 vertices per triangle * 3 coords = 9 floats per triangle
    oss << "\"vertices\":[";
    for (size_t i = 0; i < m.triangles.size(); ++i) {
        const auto& t = m.triangles[i];
        if (i > 0) oss << ",";
        oss << t.v1.x << "," << t.v1.y << "," << t.v1.z << ","
            << t.v2.x << "," << t.v2.y << "," << t.v2.z << ","
            << t.v3.x << "," << t.v3.y << "," << t.v3.z;
    }
    oss << "],";

    oss << "\"normals\":[";
    for (size_t i = 0; i < m.triangles.size(); ++i) {
        const auto& t = m.triangles[i];
        if (i > 0) oss << ",";
        // 3 normal copies per vertex for flat-shading
        for (int v = 0; v < 3; ++v) {
            if (v > 0) oss << ",";
            oss << t.normal.x << "," << t.normal.y << "," << t.normal.z;
        }
    }
    oss << "]";
    oss << "}";

    return oss.str();
}

} // namespace workbench::parsers
