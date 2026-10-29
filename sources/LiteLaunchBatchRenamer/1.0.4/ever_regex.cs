using System;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Globalization;

// Private name-computation worker; Pascal IO is available only through its explicit option.
// Original .NET Regex syntax/options/replacement groups are preserved, with a 2s timeout.
internal static class EverRegex {
    private const int Limit = 4 * 1024 * 1024;
    private static readonly UTF8Encoding Utf8 = new UTF8Encoding(false, true);
    private static string ReadString(BinaryReader reader) {
        int count = reader.ReadInt32();
        if (count < 0 || count > Limit) throw new InvalidDataException("文本过长。");
        byte[] data = reader.ReadBytes(count);
        if (data.Length != count) throw new EndOfStreamException();
        return Utf8.GetString(data);
    }
    private static void WriteString(BinaryWriter writer, string text) {
        byte[] bytes = Utf8.GetBytes(text);
        if (bytes.Length > Limit) throw new InvalidDataException("替换结果过长。");
        writer.Write(bytes.Length); writer.Write(bytes);
    }
    private static string Decode(string text) {
        // Quicker's "转义" controls interpret escape sequences; they do not Regex.Escape a pattern.
        return Regex.Unescape(text);
    }
    [STAThread]
    private static int Main() {
        AppDomain.CurrentDomain.ProcessExit += delegate { RenamerMetadata.Shutdown(); };
        using (BinaryReader reader = new BinaryReader(Console.OpenStandardInput(), Utf8))
        using (BinaryWriter writer = new BinaryWriter(Console.OpenStandardOutput(), Utf8)) {
            while (true) {
                int flags;
                try { flags = reader.ReadInt32(); } catch (EndOfStreamException) { return 0; }
                if (flags == -1) return 0;
                string pattern, replacement, input;
                try { pattern = ReadString(reader); replacement = ReadString(reader); input = ReadString(reader); }
                catch (Exception) { return 2; }
                try {
                    string result;
                    if ((flags & 1024) != 0) {
                        result = RenamerRules.Process(pattern, input);
                    } else if ((flags & 512) != 0) {
                        string[] fields = replacement.Split('|');
                        decimal start = Decimal.Parse(pattern, CultureInfo.CurrentCulture);
                        decimal ordinal = Decimal.Parse(fields[0], CultureInfo.InvariantCulture);
                        decimal total = Decimal.Parse(fields[1], CultureInfo.InvariantCulture);
                        decimal step = Decimal.Parse(fields[2], CultureInfo.InvariantCulture);
                        bool arrange = fields[3] == "1"; int width = Int32.Parse(fields[4], CultureInfo.InvariantCulture);
                        decimal value = start + ordinal * step;
                        result = value.ToString(CultureInfo.CurrentCulture);
                        if (width == -1) width = pattern.Length;
                        else if (width == 0 && pattern.StartsWith("0", StringComparison.Ordinal)) {
                            width = (total + start).ToString(CultureInfo.CurrentCulture).Length;
                            if (arrange && width == start.ToString(CultureInfo.CurrentCulture).Length) ++width;
                        }
                        if (width > 0) result = result.PadLeft(width, '0');
                    } else if ((flags & 256) != 0) {
                        DateTime value = DateTime.ParseExact(input, "yyyy-MM-ddTHH:mm:ss.fff", CultureInfo.InvariantCulture, DateTimeStyles.None);
                        result = value.ToString(pattern, CultureInfo.CurrentCulture);
                    } else {
                    if ((flags & 16) != 0) pattern = Decode(pattern);
                    if ((flags & 32) != 0) replacement = Decode(replacement);
                    RegexOptions options = RegexOptions.None;
                    if ((flags & 2) != 0) options |= RegexOptions.IgnoreCase;
                    if ((flags & 4) != 0) options |= RegexOptions.Singleline;
                    if ((flags & 8) != 0) options |= RegexOptions.Multiline;
                    bool isRegex = (flags & 1) != 0;
                    Regex expression = new Regex(isRegex ? pattern : Regex.Escape(pattern), options, TimeSpan.FromSeconds(2));
                    if ((flags & 64) != 0) {
                        Match match = expression.Match(input, Int32.Parse(replacement, CultureInfo.InvariantCulture));
                        result = (match.Success ? match.Index : -1).ToString(CultureInfo.InvariantCulture) + "\n" + (match.Success ? match.Length : 0).ToString(CultureInfo.InvariantCulture);
                    } else result = isRegex ? expression.Replace(input, replacement) : expression.Replace(input, delegate(Match m) { return replacement; });
                    }
                    if (Utf8.GetByteCount(result) > Limit) throw new InvalidDataException("替换结果超过4MB。");
                    writer.Write(1); WriteString(writer, result); writer.Flush();
                } catch (RegexMatchTimeoutException) {
                    writer.Write(0); WriteString(writer, "正则匹配超过2秒，已停止。请简化表达式。"); writer.Flush();
                } catch (Exception error) {
                    writer.Write(0); WriteString(writer, "规则计算失败：" + error.Message); writer.Flush();
                }
            }
        }
    }
}
