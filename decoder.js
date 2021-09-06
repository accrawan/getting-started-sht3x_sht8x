function decodeUplink(input) {
  var highT = input.bytes[1];
  var lowT = input.bytes[0];
  var encTemp = ((highT & 0xff) << 8) | (lowT & 0xff);
  // assume highT = 0x85, lowT = 0x43
  // highT & 0xff = 1000 0101 & 1111 1111
  // lowT & 0xff =  0100 0011 & 1111 1111
  // highTNew = 1000 0101 0000 0000 = 0x8500
  // lowTNew =  0000 0000 0100 0011 = 0x0043
  // encTemp =  1000 0101 0100 0011 = 0x8543
  // encTemp = highTNew | lowTNew
  var highH = input.bytes[3];
  var lowH = input.bytes[2];
  var encHum = ((highH & 0xff) << 8) | (lowH & 0xff);
  return {
    data: {
      bytes: input.bytes,
      temperature: sflt162f(encTemp) * 100, // multiply by 100 to get data back
      humidity: sflt162f(encHum) * 100,
    },
    warnings: [],
    errors: [],
  };
}
// see https://github.com/mcci-catena/arduino-lmic#sflt16
function sflt162f(rawSflt16) {
  // rawSflt16 is the 2-byte number decoded from wherever;
  // it's in range 0..0xFFFF
  // bit 15 is the sign bit
  // bits 14..11 are the exponent
  // bits 10..0 are the the mantissa. Unlike IEEE format,
  // the msb is explicit; this means that numbers
  // might not be normalized, but makes coding for
  // underflow easier.
  // As with IEEE format, negative zero is possible, so
  // we special-case that in hopes that JavaScript will
  // also cooperate.
  //
  // The result is a number in the open interval (-1.0, 1.0);
  //

  // throw away high bits for repeatability.
  rawSflt16 &= 0xffff;

  // special case minus zero:
  if (rawSflt16 == 0x8000) return -0.0;

  // extract the sign.
  var sSign = (rawSflt16 & 0x8000) != 0 ? -1 : 1;

  // extract the exponent
  var exp1 = (rawSflt16 >> 11) & 0xf;

  // extract the "mantissa" (the fractional part)
  var mant1 = (rawSflt16 & 0x7ff) / 2048.0;

  // convert back to a floating point number. We hope
  // that Math.pow(2, k) is handled efficiently by
  // the JS interpreter! If this is time critical code,
  // you can replace by a suitable shift and divide.
  var f_unscaled = sSign * mant1 * Math.pow(2, exp1 - 15);

  return f_unscaled;
}
