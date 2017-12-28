#include "visualdl/logic/sdk.h"

namespace visualdl {

namespace components {

template <typename T>
std::vector<T> ScalarReader<T>::records() const {
  std::vector<T> res;
  for (int i = 0; i < reader_.total_records(); i++) {
    res.push_back(reader_.record(i).data<T>(0).Get());
  }
  return res;
}

template <typename T>
std::vector<T> ScalarReader<T>::ids() const {
  std::vector<T> res;
  for (int i = 0; i < reader_.total_records(); i++) {
    res.push_back(reader_.record(i).id());
  }
  return res;
}

template <typename T>
std::vector<T> ScalarReader<T>::timestamps() const {
  std::vector<T> res;
  for (int i = 0; i < reader_.total_records(); i++) {
    res.push_back(reader_.record(i).timestamp());
  }
  return res;
}

template <typename T>
std::string ScalarReader<T>::caption() const {
  CHECK(!reader_.captions().empty()) << "no caption";
  return reader_.captions().front();
}

template <typename T>
size_t ScalarReader<T>::size() const {
  return reader_.total_records();
}

template class ScalarReader<int>;
template class ScalarReader<int64_t>;
template class ScalarReader<float>;
template class ScalarReader<double>;

void Image::StartSampling() {
  if (!ToSampleThisStep()) return;

  step_ = writer_.AddRecord();
  step_.SetId(step_id_);

  time_t time = std::time(nullptr);
  step_.SetTimeStamp(time);

  // resize record
  for (int i = 0; i < num_samples_; i++) {
    step_.AddData<value_t>();
  }
  num_records_ = 0;
}

int Image::IsSampleTaken() {
  if (!ToSampleThisStep()) return -1;
  num_records_++;
  if (num_records_ <= num_samples_) {
    return num_records_ - 1;
  }
  float prob = float(num_samples_) / num_records_;
  float randv = (float)rand() / RAND_MAX;
  if (randv < prob) {
    // take this sample
    int index = rand() % num_samples_;
    return index;
  }
  return -1;
}

void Image::FinishSampling() {
  step_id_++;
  if (ToSampleThisStep()) {
    // TODO(ChunweiYan) much optimizement here.
    writer_.parent()->PersistToDisk();
  }
}

template <typename T, typename U>
struct is_same_type {
  static const bool value = false;
};
template <typename T>
struct is_same_type<T, T> {
  static const bool value = true;
};

void Image::SetSample(int index,
                      const std::vector<shape_t>& shape,
                      const std::vector<value_t>& data) {
  // production
  int size = std::accumulate(
      shape.begin(), shape.end(), 1., [](float a, float b) { return a * b; });
  CHECK_GT(size, 0);
  CHECK_EQ(size, data.size()) << "image's shape not match data";
  CHECK_LT(index, num_samples_);
  CHECK_LE(index, num_records_);

  // set data
  auto entry = step_.MutableData<std::vector<char>>(index);
  // trick to store int8 to protobuf
  std::vector<char> data_str(data.size());
  for (int i = 0; i < data.size(); i++) {
    data_str[i] = data[i];
  }
  entry.Set(data_str);

  static_assert(
      !is_same_type<value_t, shape_t>::value,
      "value_t should not use int64_t field, this type is used to store shape");

  // set meta with hack
  Entry<shape_t> meta;
  meta.set_parent(entry.parent());
  meta.entry = entry.entry;
  meta.SetMulti(shape);
}

std::string ImageReader::caption() {
  CHECK_EQ(reader_.captions().size(), 1);
  auto caption = reader_.captions().front();
  if (Reader::TagMatchMode(caption, mode_)) {
    return Reader::GenReadableTag(mode_, caption);
  }
  string::TagDecode(caption);
  return caption;
}

ImageReader::ImageRecord ImageReader::record(int offset, int index) {
  ImageRecord res;
  auto record = reader_.record(offset);
  auto data_entry = record.data<std::vector<char>>(index);
  auto shape_entry = record.data<shape_t>(index);
  auto data_str = data_entry.Get();
  std::transform(data_str.begin(),
                 data_str.end(),
                 std::back_inserter(res.data),
                 [](char i) { return (int)((unsigned char)i); });
  res.shape = shape_entry.GetMulti();
  res.step_id = record.id();
  return res;
}

}  // namespace components

}  // namespace visualdl
