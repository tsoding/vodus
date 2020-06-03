template <typename T>
struct Maybe
{
    bool has_value;
    T unwrap;
};

#define ASSIGN_UNWRAP(place, maybe)               \
    do {                                          \
        auto __x = (maybe);                       \
        if (!__x.has_value) return {};            \
        (place) = __x.unwrap;                     \
    } while (0)

template <typename Integer>
Maybe<Integer> char_to_hex(char x)
{
    if ('0' <= x && x <= '9') {
        return {true, (Integer) (x - '0')};
    } else if ('a' <= x && x <= 'f') {
        return {true, (Integer) (x - 'a' + 10)};
    } else if ('A' <= x && x <= 'F') {
        return {true, (Integer) (x - 'A' + 10)};
    }

    return {};
}

// TODO(#20): String_View does not support unicode
struct String_View
{
    size_t count;
    const char *data;

    [[nodiscard]]
    String_View trim_begin(void) const
    {
        String_View view = *this;

        while (view.count != 0 && isspace(*view.data)) {
            view.data  += 1;
            view.count -= 1;
        }
        return view;
    }

    [[nodiscard]]
    String_View trim_end(void) const
    {
        String_View view = *this;

        while (view.count != 0 && isspace(*(view.data + view.count - 1))) {
            view.count -= 1;
        }
        return view;
    }

    [[nodiscard]]
    String_View trim(void) const
    {
        return trim_begin().trim_end();
    }

    void chop_back(size_t n)
    {
        count -= n < count ? n : count;
    }

    void chop(size_t n)
    {
        if (n > count) {
            data += count;
            count = 0;
        } else {
            data  += n;
            count -= n;
        }
    }

    String_View chop_by_delim(char delim)
    {
        assert(data);

        size_t i = 0;
        while (i < count && data[i] != delim) i++;
        String_View result = {i, data};
        chop(i + 1);

        return result;
    }

    String_View chop_word(void)
    {
        *this = trim_begin();

        size_t i = 0;
        while (i < count && !isspace(data[i])) i++;

        String_View result = { i, data };

        count -= i;
        data  += i;

        return result;
    }

    template <typename Integer>
    Maybe<Integer> from_hex() const
    {
        Integer result = {};

        for (size_t i = 0; i < count; ++i) {
            Integer x = {};
            ASSIGN_UNWRAP(x, char_to_hex<Integer>(data[i]));
            result = result * (Integer) 0x10 + x;
        }

        return {true, result};
    }

    template <typename Integer>
    Maybe<Integer> as_integer() const
    {
        Integer sign = 1;
        Integer number = {};
        String_View view = *this;

        if (view.count == 0) {
            return {};
        }

        if (*view.data == '-') {
            sign = -1;
            view.chop(1);
        }

        while (view.count) {
            if (!isdigit(*view.data)) {
                return {};
            }
            number = number * 10 + (*view.data - '0');
            view.chop(1);
        }

        return { true, number * sign };
    }

    String_View subview(size_t start, size_t count) const
    {
        if (start + count <= this->count) {
            return {count, data + start};
        }

        return {};
    }
};

String_View cstr_as_string_view(const char *cstr)
{
    return {strlen(cstr), cstr};
}

const char *string_view_as_cstr(String_View s)
{
    char *cstr = new char[s.count + 1];
    memcpy(cstr, s.data, s.count);
    cstr[s.count] = '\0';
    return cstr;
}

String_View operator ""_sv (const char *data, size_t count)
{
    String_View result;
    result.count = count;
    result.data = data;
    return result;
}

void print1(FILE *stream, String_View view)
{
    fwrite(view.data, 1, view.count, stream);
}

bool operator==(String_View view1, String_View view2)
{
    if (view1.count != view2.count) return false;
    return memcmp(view1.data, view2.data, view1.count) == 0;
}

String_View file_as_string_view(const char *filepath)
{
    assert(filepath);

    size_t n = 0;
    String_View result = {};
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        println(stderr, "Could not open file `", filepath, "`: ",
                strerror(errno));
        abort();
    }

    int code = fseek(f, 0, SEEK_END);
    if (code < 0) {
        println(stderr, "Could find the end of file ", filepath, ": ",
                strerror(errno));
        abort();
    }

    long m = ftell(f);
    if (m < 0) {
        println(stderr, "Could get the end of file ", filepath, ": ",
                strerror(errno));
        abort();
    }
    result.count = (size_t) m;

    code = fseek(f, 0, SEEK_SET);
    if (code < 0) {
        println(stderr, "Could not find the beginning of file ", filepath, ": ",
                strerror(errno));
        abort();
    }

    char *buffer = new char[result.count];
    if (!buffer) {
        println(stderr, "Could not allocate memory for file ", filepath, ": ",
                strerror(errno));
        abort();
    }

    n = fread(buffer, 1, result.count, f);
    if (n != result.count) {
        println(stderr, "Could not read file ", filepath, ": ",
                strerror(errno));
        abort();
    }

    result.data = buffer;

    return result;
}
