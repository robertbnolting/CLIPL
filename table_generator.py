s = ""

for i in range(256):
	if i % 10 == 0:
		s += "\n"
	if i in [42, 43, 45, 47, 61]:					# Operator
		s += "0020, "
	elif i in [40, 41, 44, 46, 59, 91, 93, 123, 125]:		# Separator
		s += "0040, "
	elif i in range(48, 58):						# Digits
		s += "0010, "
	elif i in range(65, 91):						# Upper case letters
		s += "0002, "
	elif i in range(97, 123):						# Lower case letters
		s += "0004, "
	else:
		if i < 255:
			s += "0000, "
		else:
			s += "0000"

print(s)
