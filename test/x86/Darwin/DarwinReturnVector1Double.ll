; ABI: i386-apple-darwin9
; FUNCTION-TYPE: <1 x double> ()

declare i64 @callee()

define i64 @caller() {
  %coerce1 = alloca <1 x double>, align 8
  %coerce = alloca <1 x double>, align 8
  %1 = call i64 @callee()
  %2 = bitcast <1 x double>* %coerce to i64*
  store i64 %1, i64* %2, align 1
  %3 = load <1 x double>* %coerce, align 8
  store <1 x double> %3, <1 x double>* %coerce1, align 8
  %4 = bitcast <1 x double>* %coerce1 to i64*
  %5 = load i64* %4, align 1
  ret i64 %5
}
