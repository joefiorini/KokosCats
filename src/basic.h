namespace koko {
class Basic {
public:
  constexpr Basic() = default;
  constexpr int add(int n1, int n2) const { return n1 + n2; }
};

} // namespace koko
