#include "security/argon.hpp"

TEST(Argon2Test, VerifiesEncodedArgon2idHash) {
	constexpr std::string_view hash = "$argon2id$v=19$m=65536,t=2,p=2$MDEyMzQ1Njc4OWFiY2RlZg$bXQFphhZPU5gOn9WEZTSQhgh3m0sfKXsUNSs44OaJ08";

	EXPECT_TRUE(Argon2 {}.argon("correct horse battery staple", std::string(hash)));
	EXPECT_FALSE(Argon2 {}.argon("wrong password", std::string(hash)));
	EXPECT_FALSE(Argon2 {}.argon("password", "5baa61e4c9b93f3f0682250b6cf8331b7ee68fd8"));
}
