static TC_CHAR_TYPES = [
  1,1,1,1,1,1,1,1,1,32,32,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  32,10,1,1,1,1,11,39,40,41,6,4,44,16,46,7,2,2,2,2,2,2,2,2,2,2,58,59,12,13,
  14,1,1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,91,1,93,1,3,
  1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,123,15,125,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1];

static TC_OPERATORS = [
  1,1,16,1,8,18,1,1,12,10,15,1,61,13,13,1,14,11,17,11,9,19,1,1,5,1,1,20];

static TC_KEYWORDS = [
  0,0,0,0,0,0,0,107,88,0,0,0,0,0,0,0,0,0,0,0,0,137,
  0,0,121,0,0,96,0,92,0,0,0,0,0,75,101,0,0,0,0,0,0,0,
  0,0,129,0,113,150,64,145,0,0,0,0,0,82,0,0,0,0,0,68,
  2,'a','s',138,
  5,'b','r','e','a','k',128,
  5,'c','o','n','s','t',129,
  4,'e','l','s','e',130,
  2,'f','n',131,
  2,'i','f',132,
  3,'l','e','t',133,
  4,'l','o','o','p',134,
  4,'n','u','l','l',139,
  6,'r','e','t','u','r','n',135,
  6,'s','i','z','e','o','f',140,
  6,'s','t','a','t','i','c',136,
  6,'s','t','r','u','c','t',141,
  3,'u','6','4',142,
  5,'w','h','i','l','e',137];

fn mem_compare(ptr1: &u64, ptr2: &u64, size: u64) -> u64 {
  let i = 0;
  while i < size && load8(ptr1 + i) == load8(ptr2 + i) {
    i = i + 1;
  }
  return size - i;
}

fn tc_get_keyword(start: &u64, length: u64, hashcode: u64) -> u64 {
  let keyword = load8(TC_KEYWORDS + hashcode);
  if keyword != 0 &&
    length == load8(TC_KEYWORDS + keyword) &&
    mem_compare(start, TC_KEYWORDS + keyword + 1, length) == 0 {
      return load8(TC_KEYWORDS + keyword + length + 1);
  }
  return TC_IDENTIFIER;
}

fn tc_read_char(self: &Compiler) -> u64 {
  let src = self.src;
  let src_end = self.src_end;
  if src >= src_end { panic(10); }
  src = src + 1;
  let c = 0;
  let type = 0;
  if src < src_end {
    c = load8(src);
    type = load8(TC_CHAR_TYPES + c);
  }
  self.src = src;
  self.next_char = c;
  self.next_char_type = type;
  return type;
}

fn tc_read_integer(self: &Compiler) -> u64 {
  let type = self.next_char_type;
  let v = 0;
  while type == TC_INTEGER {
    v = v * 10 + (self.next_char - '0');
    type = tc_read_char(self);
  }
  self.next_token_data = v;
  return TC_INTEGER;
}

fn tc_read_quoted_char(self: &Compiler) -> u64 {
  tc_read_char(self);
  let value = self.next_char;
  if value < 32 || value >= 127 { panic(11); }
  if tc_read_char(self) != ''' { panic(12); }
  tc_read_char(self);
  self.next_token_data = value;
  return TC_INTEGER;
}

fn tc_read_identifier(self: &Compiler) -> u64 {
  let hashcode = 0;
  let start = self.src;
  let type = self.next_char_type;
  while type == TC_IDENTIFIER || type == TC_INTEGER {
    hashcode = 31 * hashcode + self.next_char;
    type = tc_read_char(self);
  }
  let length = self.src - start;
  self.next_token_data = start as u64;
  self.next_token_length = length;
  return tc_get_keyword(start, length, hashcode & 63);
}

fn tc_read_operator(self: &Compiler, first_char_type: u64) -> u64 {
  let second_char_type = tc_read_char(self);
  let index = 4 * (first_char_type - 10);
  if second_char_type == first_char_type {
    tc_read_char(self);
    index = index + 1;
  } else if self.next_char == '=' {
    tc_read_char(self);
    index = index + 2;
  } else if self.next_char == '>' {
    tc_read_char(self);
    index = index + 3;
  }
  return load8(TC_OPERATORS + index);
}

fn tc_read_comment(self: &Compiler, src: &u64) -> u64 {
  if src + 1 >= self.src_end || load8(src + 1) != '*' { return 0; }
  while src + 3 < self.src_end && load16(src + 2) != 12074 {
    src = src + 1;
  }
  self.src = src + 3; /* The last '/' is NOT read. */
  return 1;
}

fn tc_read_token(self: &Compiler) {
  let type = self.next_char_type;
  while type == ' ' || type == TC_DIV {
    if type == TC_DIV && tc_read_comment(self, self.src) == 0 { break; }
    type = tc_read_char(self);
  }
  let token = type;
  if type == TC_INTEGER {
    token = tc_read_integer(self);
  } else if type == ''' {
    token = tc_read_quoted_char(self);
  } else if type == TC_IDENTIFIER {
    token = tc_read_identifier(self);
  } else if type >= 10 && type < 20 {
    token = tc_read_operator(self, type);
  } else if type != 0 {
    tc_read_char(self);
  }
  self.next_token = token;
}
