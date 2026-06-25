#include "engine.hpp"
#include <cstdio>
#include <string>
#include <vector>

using namespace vgo;

struct Case { const char* keys; const char* want; };

// Mặc định: oldStyleTone = true (hòa / thúy / khoẻ)
static const std::vector<Case> CASES = {
    // --- thanh cơ bản ---
    {"as", "á"}, {"af", "à"}, {"ar", "ả"}, {"ax", "ã"}, {"aj", "ạ"},
    // --- dấu phụ ---
    {"aa", "â"}, {"ee", "ê"}, {"oo", "ô"},
    {"aw", "ă"}, {"ow", "ơ"}, {"uw", "ư"}, {"w", "w"}, {"tuw", "tư"},
    {"dd", "đ"},
    // --- thanh + dấu phụ ---
    {"aas", "ấ"}, {"aaf", "ầ"}, {"ees", "ế"}, {"oos", "ố"},
    {"aws", "ắ"}, {"ows", "ớ"}, {"uws", "ứ"},
    // --- từ thường ---
    {"vieejt", "việt"}, {"tieengs", "tiếng"}, {"nguwowif", "người"},
    {"ddaaus", "đấu"}, {"hoo", "hô"}, {"mej", "mẹ"},
    {"chaof", "chào"}, {"ddi", "đi"}, {"ddepj", "đẹp"},
    {"camr", "cảm"}, {"ows", "ớ"}, {"thuwowngr", "thưởng"},
    // --- đặt dấu: nguyên âm có mũ/móc ưu tiên ---
    {"uoongs", "uống"}, {"dduwowngf", "đường"}, {"giuwax", "giữa"},
    {"khuyeenr", "khuyển"}, {"yeeu", "yêu"},
    // --- qu / gi ---
    {"quas", "quá"}, {"quys", "quý"}, {"quyeesn", "quyến"},
    {"gif", "gì"}, {"giaf", "già"}, {"giuwowngf", "giường"},
    // --- oa/oe/uy kiểu cũ (dấu về nguyên âm đầu) ---
    {"hoaf", "hòa"}, {"khoer", "khỏe"}, {"thuys", "thúy"}, {"thuyr", "thủy"},
    // --- dời dấu khi thêm phụ âm cuối ---
    {"toansr", "toản"},  // t,o,a,n,s,r ? kiểm chuỗi dài
    {"loanx", "loãn"}, {"toans", "toán"}, {"uons", "uón"},
    {"toasn", "toán"},  // gõ dấu trước phụ âm cuối -> phải dời
    // --- gõ đúp phím điều khiển ---
    {"aaa", "aa"},      // mũ (aa->â) gõ lần 3: revert -> "aa"
    {"thijj", "thị"},   // dấu gõ đúp (fat-finger): lờ phím thừa -> giữ "thị"
    {"ddd", "ddd"},     // "dd" không phải tiền tố phụ âm đầu -> thô
    {"aww", "aww"},     // "aw": coda 'w' không hợp lệ -> thô
    // --- tiếng Anh lẫn lộn: phải GIỮ NGUYÊN (không mangle) ---
    {"browser", "browser"}, {"file", "file"}, {"password", "password"},
    {"video", "video"}, {"data", "data"}, {"google", "google"},
    {"internet", "internet"}, {"javascript", "javascript"}, {"email", "email"},
    {"server", "server"}, {"debug", "debug"}, {"folder", "folder"},
    {"the", "the"}, {"they", "they"},
    // câu trộn Anh-Việt
    {"guwir email cho sysadmin", "gửi email cho sysadmin"},
    // --- giữ hoa ---
    {"Tieengs", "Tiếng"}, {"Vieejt", "Việt"}, {"DDays", "Đáy"},
    // --- nhiều từ (space commit) ---
    {"tieengs vieejt", "tiếng việt"},
    {"xin chaof banj", "xin chào bạn"},
};

int main() {
    Config cfg; // oldStyleTone = true
    int pass = 0, fail = 0;
    for (auto& c : CASES) {
        std::string got = typeString(c.keys, cfg);
        bool ok = (got == c.want);
        if (ok) ++pass;
        else {
            ++fail;
            std::printf("FAIL  %-16s -> got [%s]  want [%s]\n", c.keys, got.c_str(), c.want);
        }
    }
    // --- từ điển tiếng Anh + VN priority (nạp từ vào dict rồi kiểm) ---
    dictAddWord("test");
    dictAddWord("this");
    dictAddWord("cos");   // cố tình thêm để chắc VN_PRIORITY vẫn thắng
    dictAddWord("been");  // "been" trong dict Anh...
    vnAddWord("been");    // ...nhưng ép-Việt ưu tiên -> phải ra "bên"
    const std::vector<Case> DCASES = {
        {"test", "test"},   // trong dict -> giữ thô, không thành "tét"
        {"this", "this"},   // -> không thành "thí"
        {"cos",  "có"},     // VN_PRIORITY: dù có trong dict vẫn ra "có"
        {"browser", "browser"},  // vẫn bắt theo cấu trúc (không cần dict)
        {"been", "bên"},    // ép-Việt thắng từ điển Anh
    };
    for (auto& c : DCASES) {
        std::string got = typeString(c.keys, cfg);
        if (got == c.want) ++pass;
        else { ++fail; std::printf("FAIL(dict) %-10s -> got [%s]  want [%s]\n",
                                   c.keys, got.c_str(), c.want); }
    }

    std::printf("\n%d passed, %d failed (%d total)\n", pass, fail, pass + fail);
    return fail ? 1 : 0;
}
