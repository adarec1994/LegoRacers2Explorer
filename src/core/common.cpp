std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return text;
}

Vec3 Add(Vec3 lhs, Vec3 rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 Subtract(Vec3 lhs, Vec3 rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 Cross(Vec3 lhs, Vec3 rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

float Dot(Vec3 lhs, Vec3 rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

float Length(Vec3 value) {
    return std::sqrt(Dot(value, value));
}

Vec3 Normalize(Vec3 value) {
    const float length = Length(value);
    if (length <= 0.00001f || !std::isfinite(length)) {
        return {};
    }
    return {value.x / length, value.y / length, value.z / length};
}

Vec3 Multiply(Vec3 value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float DistanceSquared(Vec3 lhs, Vec3 rhs) {
    const Vec3 delta = Subtract(lhs, rhs);
    return Dot(delta, delta);
}

Quat Normalize(Quat value) {
    const float length = std::sqrt(
        value.x * value.x +
        value.y * value.y +
        value.z * value.z +
        value.w * value.w);
    if (length <= 0.00001f || !std::isfinite(length)) {
        return {};
    }
    return {value.x / length, value.y / length, value.z / length, value.w / length};
}

Quat Conjugate(Quat value) {
    return {-value.x, -value.y, -value.z, value.w};
}

Quat Inverse(Quat value) {
    return Conjugate(Normalize(value));
}

Quat QuatFromAxisAngle(Vec3 axis, float angle) {
    axis = Normalize(axis);
    const float halfAngle = angle * 0.5f;
    const float sine = std::sin(halfAngle);
    return Normalize({axis.x * sine, axis.y * sine, axis.z * sine, std::cos(halfAngle)});
}

Quat Multiply(Quat lhs, Quat rhs) {
    return Normalize({
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
    });
}

Quat Slerp(Quat start, Quat end, float t) {
    start = Normalize(start);
    end = Normalize(end);
    t = std::clamp(t, 0.0f, 1.0f);

    float dot =
        start.x * end.x +
        start.y * end.y +
        start.z * end.z +
        start.w * end.w;
    if (dot < 0.0f) {
        dot = -dot;
        end = {-end.x, -end.y, -end.z, -end.w};
    }

    if (dot > 0.9995f) {
        return Normalize({
            start.x + (end.x - start.x) * t,
            start.y + (end.y - start.y) * t,
            start.z + (end.z - start.z) * t,
            start.w + (end.w - start.w) * t,
        });
    }

    const float theta0 = std::acos(std::clamp(dot, -1.0f, 1.0f));
    const float theta = theta0 * t;
    const float sinTheta = std::sin(theta);
    const float sinTheta0 = std::sin(theta0);
    if (std::abs(sinTheta0) <= 0.00001f) {
        return start;
    }

    const float scaleStart = std::cos(theta) - dot * sinTheta / sinTheta0;
    const float scaleEnd = sinTheta / sinTheta0;
    return Normalize({
        start.x * scaleStart + end.x * scaleEnd,
        start.y * scaleStart + end.y * scaleEnd,
        start.z * scaleStart + end.z * scaleEnd,
        start.w * scaleStart + end.w * scaleEnd,
    });
}

Vec3 Rotate(Quat rotation, Vec3 value) {
    rotation = Normalize(rotation);
    const Vec3 q{rotation.x, rotation.y, rotation.z};
    const Vec3 t = {
        2.0f * (q.y * value.z - q.z * value.y),
        2.0f * (q.z * value.x - q.x * value.z),
        2.0f * (q.x * value.y - q.y * value.x),
    };
    return {
        value.x + rotation.w * t.x + (q.y * t.z - q.z * t.y),
        value.y + rotation.w * t.y + (q.z * t.x - q.x * t.z),
        value.z + rotation.w * t.z + (q.x * t.y - q.y * t.x),
    };
}

float GetAxis(Vec3 value, int axis) {
    switch (axis) {
    case 0:
        return value.x;
    case 1:
        return value.y;
    case 2:
    default:
        return value.z;
    }
}

void SetAxis(Vec3& value, int axis, float component) {
    switch (axis) {
    case 0:
        value.x = component;
        break;
    case 1:
        value.y = component;
        break;
    case 2:
    default:
        value.z = component;
        break;
    }
}

float DistancePointSegmentSquared(Vec3 point, Vec3 start, Vec3 end) {
    const Vec3 segment = Subtract(end, start);
    const float segmentLengthSquared = Dot(segment, segment);
    if (segmentLengthSquared <= 0.000001f) {
        return DistanceSquared(point, start);
    }

    const float t = std::clamp(Dot(Subtract(point, start), segment) / segmentLengthSquared, 0.0f, 1.0f);
    const Vec3 closest = Add(start, Multiply(segment, t));
    return DistanceSquared(point, closest);
}

std::vector<std::string> SplitArchivePath(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');
    std::vector<std::string> parts;
    std::string current;
    for (const char character : path) {
        if (character == '\\') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(character);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

std::string JoinArchivePath(const std::string& parent, const std::string& name) {
    if (parent.empty()) {
        return name;
    }
    return parent + "\\" + name;
}

std::string SettingsFilePathString();

std::string PreferredInitialDirectory() {
    std::ifstream settings(SettingsFilePathString());
    std::string savedPath;
    std::getline(settings, savedPath);
    const std::filesystem::path saved(savedPath);
    if (!saved.empty() && saved.has_parent_path() && std::filesystem::exists(saved.parent_path())) {
        return saved.parent_path().string();
    }

    const std::filesystem::path lr2 = L"C:\\Users\\pwd12\\OneDrive\\Documents\\lr2";
    if (std::filesystem::exists(lr2)) {
        return lr2.string();
    }
    return std::filesystem::current_path().string();
}

std::filesystem::path SettingsFilePath() {
    if (const char* appData = std::getenv("APPDATA"); appData != nullptr && appData[0] != '\0') {
        return std::filesystem::path(appData) / "LegoRacers2GtcBrowser" / "settings.txt";
    }
    return std::filesystem::current_path() / "LegoRacers2GtcBrowser.settings.txt";
}

std::string SettingsFilePathString() {
    return SettingsFilePath().string();
}

std::string LoadSavedGtcPath() {
    std::ifstream file(SettingsFilePath());
    std::string path;
    std::getline(file, path);
    return path;
}

void SaveSavedGtcPath(const std::filesystem::path& path) {
    try {
        const auto settingsPath = SettingsFilePath();
        if (settingsPath.has_parent_path()) {
            std::filesystem::create_directories(settingsPath.parent_path());
        }

        std::ofstream file(settingsPath, std::ios::trunc);
        if (file) {
            file << path.string();
        }
    } catch (...) {
    }
}
