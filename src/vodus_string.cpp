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
};

String_View cstr_as_string_view(const char *cstr)
{
    return {strlen(cstr), cstr};
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
