"use strict";

function hasNullableRepeat(source) {
  let index = 0;
  let found = false;

  function take() {
    const codePoint = source.codePointAt(index);
    if (codePoint === undefined) return "";
    const character = String.fromCodePoint(codePoint);
    index += character.length;
    return character;
  }

  function expression() {
    let nullable = sequence();
    while (source[index] === "|") {
      index++;
      nullable = sequence() || nullable;
    }
    return nullable;
  }

  function sequence() {
    let nullable = true;
    while (index < source.length && source[index] !== ")" && source[index] !== "|") {
      const operandNullable = atom();
      let atomNullable = operandNullable;
      const quantifier = source[index];
      let quantified = false;
      let unbounded = false;

      if (quantifier === "*" || quantifier === "+" || quantifier === "?") {
        index++;
        quantified = true;
        unbounded = quantifier !== "?";
        if (quantifier !== "+") atomNullable = true;
      } else if (quantifier === "{") {
        const match = /^\{(\d+)(?:,(\d*))?\}/.exec(source.slice(index));
        if (match) {
          index += match[0].length;
          quantified = true;
          unbounded = match[2] === "";
          atomNullable = Number(match[1]) === 0 || atomNullable;
        }
      }
      if (unbounded && operandNullable) found = true;
      if (quantified && source[index] === "?") index++;
      nullable = nullable && atomNullable;
    }
    return nullable;
  }

  function atom() {
    const character = take();
    if (character === "(") {
      if (source.startsWith("?:", index)) index += 2;
      const nullable = expression();
      if (source[index] === ")") index++;
      return nullable;
    }
    if (character === "[") {
      if (source[index] === "^") index++;
      if (source[index] === "]") index++;
      while (index < source.length && source[index] !== "]") {
        if (take() === "\\" && index < source.length) take();
      }
      if (source[index] === "]") index++;
      return false;
    }
    if (character === "\\") {
      const escaped = take();
      return escaped === "b" || escaped === "B";
    }
    return character === "^" || character === "$";
  }

  expression();
  return found;
}

module.exports = {hasNullableRepeat};
