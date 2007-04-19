#include <cassert>

#include "Distributions.hh"
#include "str.hh"

#include "blaspp.h"
#include "blas1pp.h"
#include "blas2pp.h"
#include "blas3pp.h"


// FIXME: NOT TRIED
void
Gaussian::split(Gaussian &g1, Gaussian &g2) const
{
  assert(dim() != 0);

  Vector mean1; get_mean(mean1);
  Vector mean2; get_mean(mean2);
  Matrix cov; get_covariance(cov);

  // Add/Subtract standard deviations
  // FIXME: should we use eigvals/vecs for full covs?
  double sd=0;
  for (int i=0; i<dim(); i++) {
    sd=0.00001*sqrt(cov(i,i));
    mean1(i) -= sd;
    mean2(i) += sd;
  }
  
  g1.set_mean(mean1);
  g2.set_mean(mean2);
  g1.set_covariance(cov);
  g2.set_covariance(cov);
}


// Merge using:
// f1=n1/(n1+n2) ; f2=n2/(n1+n2)
// mu=f1*mu1+f2*mu2
// sigma=f1*sigma1+f2*sigma2+f1*f2*(mu1-mu2)^2
// FIXME: NOT TRIED
void
Gaussian::merge(double weight1, const Gaussian &m1, 
		double weight2, const Gaussian &m2)
{
  assert(m1.dim() == m2.dim());
  assert(weight1 <1 && weight2 <1 && weight1>0 && weight2>0);
  
  reset(m1.dim());

  double f1=weight1/(weight1+weight2);
  double f2=weight2/(weight1+weight2);

  // Merged mean
  Vector mu; m1.get_mean(mu);
  Vector vtemp; m2.get_mean(vtemp);
  Blas_Scale(weight1, mu);
  Blas_Add_Mult(mu, weight2, vtemp);
  set_mean(mu);

  // Merged covariance
  Matrix covariance; m1.get_covariance(covariance);
  Matrix mtemp; m2.get_covariance(mtemp);
  Blas_Scale(weight1, covariance);
  Blas_Scale(weight2, mtemp);
  // FIXME: is this ok?
  covariance = covariance + mtemp;
  Vector diff; m1.get_mean(diff);
  Blas_Add_Mult(diff, -1, vtemp);
  Blas_R1_Update(covariance, diff, diff, f1*f2);
  set_covariance(covariance);
}



// FIXME: NOT TRIED
double
Gaussian::kullback_leibler(Gaussian &g) const
{
  assert(dim()==g.dim());
  
  LaVectorLongInt pivots(dim(),1);
  LaGenMatDouble t1; g.get_covariance(t1);
  LaGenMatDouble t2; g.get_covariance(t2);
  LaVectorDouble t3; g.get_mean(t3);
  LaVectorDouble t4; g.get_mean(t4);
  LUFactorizeIP(t1, pivots);
  LaLUInverseIP(t1, pivots);
  
  
  LaGenMatDouble cov; get_covariance(cov);
  LaVectorDouble mean; get_mean(mean);
  Blas_Mat_Mat_Mult(t1, cov, t2, 1.0, 0.0);
  Blas_Add_Mult(t3, -1, mean); 
  Blas_Mat_Vec_Mult(t1, t3, t4);
  
  double value=LinearAlgebra::determinant(t1)
    /LinearAlgebra::determinant(cov);
  //  FIXME: logarithm ok?
  value = log2(value);
  value += t2.trace();
  value += Blas_Dot_Prod(t3, t4);
  value -= dim();
  value *= 0.5;

  return value;
}


// FIXME: NOT TRIED
DiagonalGaussian::DiagonalGaussian(int dim)
{
  reset(dim);
}


// FIXME: NOT TRIED
DiagonalGaussian::DiagonalGaussian(const DiagonalGaussian &g)
{
  reset(g.dim());
  g.get_mean(m_mean);
  g.get_covariance(m_covariance);
  for (int i=0; i<dim(); i++)
    m_precision(i)=1/m_covariance(i);
}


// FIXME: NOT TRIED
DiagonalGaussian::~DiagonalGaussian()
{
  if (m_accum != NULL)
    delete m_accum; 
}


// FIXME: NOT TRIED
void
DiagonalGaussian::reset(int dim)
{
  m_mean.resize(dim);
  m_covariance.resize(dim);
  m_precision.resize(dim);
  m_mean=0; m_covariance=0; m_precision=0;
  m_dim=dim;
}


// FIXME: NOT TRIED
double
DiagonalGaussian::compute_likelihood(const FeatureVec &f) const
{
  return exp(compute_log_likelihood(f));
}


// FIXME: NOT TRIED
double
DiagonalGaussian::compute_log_likelihood(const FeatureVec &f) const
{
  double ll=0;
  // FIXME!
  //  Vector diff(f);
  Vector diff(f.dim());
  for (int i=0; i<dim(); i++)
    diff(i)=f[i];
  Blas_Add_Mult(diff, -1, m_mean);
  for (int i=0; i<dim(); i++)
    ll += diff(i)*diff(i)*m_precision(i);
  ll *= -0.5;
  ll += m_constant;

  return ll;
}


// FIXME: NOT TRIED
void
DiagonalGaussian::write(std::ostream &os) const
{
  os << "diag ";
  for (int i=0; i<dim(); i++)
    os << m_mean(i) << " ";
  for (int i=0; i<dim()-1; i++)
    os << m_covariance(i) << " ";
  os << m_covariance(dim()-1);
}


// FIXME: NOT TRIED
void
DiagonalGaussian::read(std::istream &is)
{
  /*
  std::string line;
  getline(is, line);
  str::chomp(&line);

  std::vector<std::string> fields;
  str::split(&line, " ", false, &fields);
  
  int pos=0;
  std::string type("diagonal_cov");
  if (fields[0] == type)
    pos++;

  bool ok;
  for (int i=0; i<dim(); i++) {
    m_mean(i) = str::str2float(&(fields[pos]),&ok);
    pos++;
  }
  for (int i=0; i<dim()-1; i++) {
    m_covariance(i) = str::str2float(&(fields[pos]),&ok);
    pos++;
  }
  */

  for (int i=0; i<dim(); i++)
    is >> m_mean(i);
  for (int i=0; i<dim(); i++) {
    is >> m_covariance(i);
    m_precision(i) = 1/m_covariance(i);
  }
  m_constant=1;
  for (int i=0; i<dim(); i++)
    m_constant *= m_covariance(i);
  m_constant = 1/sqrt(m_constant);
  m_constant = log(m_constant);
}


// FIXME: NOT TRIED
void
DiagonalGaussian::start_accumulating()
{
  m_accum = new DiagonalAccumulator(dim());
}


// FIXME: NOT TRIED
void
DiagonalGaussian::accumulate_ml(double prior,
				const FeatureVec &f)
{
  assert(m_accum != NULL);

  for (int i=0; i<dim(); i++) {
    m_accum->ml_mean += prior*f[i];
    m_accum->ml_cov += prior*f[i]*f[i];
  }
}


// FIXME: NOT TRIED
void
DiagonalGaussian::accumulate_mmi_denominator(std::vector<double> priors,
					     std::vector<const FeatureVec*> const features)
{
  assert(m_accum != NULL);
  
  for (int i=0; i<dim(); i++)
    for (unsigned int j=0; j<priors.size(); j++) {
      // FIXME
      //      m_accum->mmi_mean += priors[j]*features[j]->[i];
      //      m_accum->mmi_cov += priors[j]*features[j]->[i]*features[j]->[i];
    }
}


// FIXME: NOT TRIED
void
DiagonalGaussian::estimate_parameters()
{
  assert(m_accum != NULL);
  
  if (m_mode == ML) {
    m_mean.copy(m_accum->ml_mean);
    m_covariance.copy(m_accum->ml_cov);
  }

  else if (m_mode == MMI) {
    // Do something smart
  }

  // Clear the accumulators
  delete m_accum;
}


// FIXME: NOT TRIED
void
DiagonalGaussian::get_mean(Vector &mean) const
{
  mean.copy(m_mean);
}


// FIXME: NOT TRIED
void
DiagonalGaussian::get_covariance(Matrix &covariance) const
{
  covariance.resize(dim(),dim());
  covariance=0;
  for (int i=0; i<dim(); i++)
    covariance(i,i)=m_covariance(i);
}


// FIXME: NOT TRIED
void
DiagonalGaussian::set_mean(const Vector &mean)
{
  assert(mean.size()==dim());
  m_mean.copy(mean);
}


// FIXME: NOT TRIED
void
DiagonalGaussian::set_covariance(const Matrix &covariance)
{
  assert(covariance.rows()==dim());
  assert(covariance.cols()==dim());
  m_covariance.copy(covariance);
}


// FIXME: NOT TRIED
void
DiagonalGaussian::get_covariance(Vector &covariance) const
{
  covariance.copy(m_covariance);
}


// FIXME: NOT TRIED
void
DiagonalGaussian::set_covariance(const Vector &covariance)
{
  assert(covariance.size()==dim());
  m_covariance.copy(covariance);
}


// FIXME: NOT TRIED
FullCovarianceGaussian::FullCovarianceGaussian(int dim)
{
  reset(dim);
}


// FIXME: NOT TRIED
FullCovarianceGaussian::FullCovarianceGaussian(const FullCovarianceGaussian &g)
{
  reset(g.dim());
  g.get_mean(m_mean);
  g.get_covariance(m_covariance);
  LinearAlgebra::inverse(m_covariance, m_precision);
}


// FIXME: NOT TRIED
FullCovarianceGaussian::~FullCovarianceGaussian()
{
  if (m_accum != NULL)
    delete m_accum;
}


// FIXME: NOT TRIED
void
FullCovarianceGaussian::reset(int dim)
{
  m_mean.resize(dim);
  m_covariance.resize(dim,dim);
  m_precision.resize(dim,dim);
  m_mean=0; m_covariance=0; m_precision=0;
}


// FIXME: NOT TRIED
double
FullCovarianceGaussian::compute_likelihood(const FeatureVec &f) const
{
  return exp(compute_log_likelihood(f));
}


// FIXME: NOT TRIED
double
FullCovarianceGaussian::compute_log_likelihood(const FeatureVec &f) const
{
  double ll=0;

  // FIXME
  //  Vector diff(f);
  Vector diff(f.dim());
  Vector t(f.dim());

  Blas_Add_Mult(diff, -1, m_mean);
  Blas_Mat_Vec_Mult(m_precision, diff, t, 1, 0);
  ll = Blas_Dot_Prod(diff, t);
  ll *= -0.5;
  ll += m_constant;

  return ll;
}


// FIXME: NOT TRIED
void
FullCovarianceGaussian::write(std::ostream &os) const
{
  os << "full ";
  for (int i=0; i<dim(); i++)
    os << m_mean(i) << " ";
  for (int i=0; i<dim(); i++)
    for (int j=0; j<dim(); j++)
      if (!(i==dim()-1 && j==dim()-1))
	os << m_covariance(i,j) << " ";
  os << m_covariance(dim()-1, dim()-1);
}


// FIXME: NOT TRIED
void
FullCovarianceGaussian::read(std::istream &is)
{
  std::string type;
  is >> type;
  if (type != "full")
    throw std::string("Error reading FullCovarianceGaussian parameters from a stream\n");
  
  std::string line;
  getline(is, line);
  str::chomp(&line);
  
  for (int i=0; i<dim(); i++)
    is >> m_mean(i);
  for (int i=0; i<dim(); i++)
    for (int j=0; j<dim(); j++)
      is >> m_covariance(i,j);
}


// FIXME: NOT TRIED
void
FullCovarianceGaussian::start_accumulating()
{
  m_accum = new FullCovarianceAccumulator(dim());
}


// FIXME: NOT TRIED
void
FullCovarianceGaussian::accumulate_ml(double prior,
				      const FeatureVec &f)
{
  assert(m_accum != NULL);
  
  // FIXME!
  //  m_accum->ml_mean(i) += prior*f;
  //  Blas_R1_Update(m_accum->ml_cov, f, f, prior);
}


// FIXME: NOT TRIED
void
FullCovarianceGaussian::accumulate_mmi_denominator(std::vector<double> priors,
						   std::vector<const FeatureVec*> const features)
{
  assert(m_accum != NULL);
  
  for (unsigned int i=0; i<priors.size(); i++) {
    // FIXME!
    //    m_accum->mmi_mean += priors[i]*features[i];
    //    Blas_R1_Update(m_accum->mmi_cov, features[i], features[i], priors[i]);
  }
}


// FIXME: NOT TRIED
void
FullCovarianceGaussian::estimate_parameters()
{
  assert(m_accum != NULL);
  
  if (m_mode == ML) {
    m_mean.copy(m_accum->ml_mean);
    m_covariance.copy(m_accum->ml_cov);
  }

  else if (m_mode == MMI) {
    // Do something smart
  }

  // Clear the accumulators
  delete m_accum;
}


// FIXME: NOT TRIED
void
FullCovarianceGaussian::get_mean(Vector &mean) const
{
  mean.copy(m_mean);
}


// FIXME: NOT TRIED
void
FullCovarianceGaussian::get_covariance(Matrix &covariance) const
{
  covariance.copy(m_covariance);
}


// FIXME: NOT TRIED
void
FullCovarianceGaussian::set_mean(const Vector &mean)
{
  assert(mean.size()==dim());
  m_mean.copy(mean);
}


// FIXME: NOT TRIED
void
FullCovarianceGaussian::set_covariance(const Matrix &covariance)
{
  assert(covariance.rows()==dim());
  assert(covariance.cols()==dim());
  m_covariance.copy(covariance);
}


PrecisionSubspace::PrecisionSubspace()
{
}


PrecisionSubspace::~PrecisionSubspace()
{
}


void
PrecisionSubspace::set_dim(int dim)
{
  m_dim=dim;
}


int
PrecisionSubspace::dim() const
{
  return m_dim;
}


// FIXME: NOT TRIED
PrecisionConstrainedGaussian::PrecisionConstrainedGaussian(int dim)
{
}


// FIXME: NOT TRIED
PrecisionConstrainedGaussian::PrecisionConstrainedGaussian(const PrecisionConstrainedGaussian &g)
{
}


// FIXME: NOT TRIED
PrecisionConstrainedGaussian::~PrecisionConstrainedGaussian()
{
}


// FIXME: NOT TRIED
void
PrecisionConstrainedGaussian::reset(int dim)
{
}


// FIXME: NOT TRIED
double
PrecisionConstrainedGaussian::compute_likelihood(const FeatureVec &f) const
{
  return 0;
}


// FIXME: NOT TRIED
double
PrecisionConstrainedGaussian::compute_log_likelihood(const FeatureVec &f) const
{
  return 0;
}


// FIXME: NOT TRIED
void
PrecisionConstrainedGaussian::write(std::ostream &os) const
{
}


// FIXME: NOT TRIED
void
PrecisionConstrainedGaussian::read(std::istream &is)
{
}


// FIXME: NOT TRIED
void
PrecisionConstrainedGaussian::start_accumulating()
{
}


// FIXME: NOT TRIED
void
PrecisionConstrainedGaussian::accumulate_ml(double prior,
				      const FeatureVec &f)
{
}


// FIXME: NOT TRIED
void
PrecisionConstrainedGaussian::accumulate_mmi_denominator(std::vector<double> priors,
						   std::vector<const FeatureVec*> const features)
{
}


// FIXME: NOT TRIED
void
PrecisionConstrainedGaussian::estimate_parameters()
{
}


// FIXME: NOT TRIED
void
PrecisionConstrainedGaussian::get_mean(Vector &mean) const
{
}


// FIXME: NOT TRIED
void
PrecisionConstrainedGaussian::get_covariance(Matrix &covariance) const
{
}


// FIXME: NOT TRIED
void
PrecisionConstrainedGaussian::set_mean(const Vector &mean)
{
}


// FIXME: NOT TRIED
void
PrecisionConstrainedGaussian::set_covariance(const Matrix &covariance)
{
}


ExponentialSubspace::ExponentialSubspace()
{
}


ExponentialSubspace::~ExponentialSubspace()
{
}


void
ExponentialSubspace::set_dim(int dim)
{
  m_dim=dim;
}


int
ExponentialSubspace::dim() const
{
  return m_dim;
}



// FIXME: NOT TRIED
SubspaceConstrainedGaussian::SubspaceConstrainedGaussian(int dim)
{
}


// FIXME: NOT TRIED
SubspaceConstrainedGaussian::SubspaceConstrainedGaussian(const SubspaceConstrainedGaussian &g)
{
}


// FIXME: NOT TRIED
SubspaceConstrainedGaussian::~SubspaceConstrainedGaussian()
{
}


// FIXME: NOT TRIED
void
SubspaceConstrainedGaussian::reset(int dim)
{
}


// FIXME: NOT TRIED
double
SubspaceConstrainedGaussian::compute_likelihood(const FeatureVec &f) const
{
  return 0;
}


// FIXME: NOT TRIED
double
SubspaceConstrainedGaussian::compute_log_likelihood(const FeatureVec &f) const
{
  return 0;
}


// FIXME: NOT TRIED
void
SubspaceConstrainedGaussian::write(std::ostream &os) const
{
}


// FIXME: NOT TRIED
void
SubspaceConstrainedGaussian::read(std::istream &is)
{
}


// FIXME: NOT TRIED
void
SubspaceConstrainedGaussian::start_accumulating()
{
}


// FIXME: NOT TRIED
void
SubspaceConstrainedGaussian::accumulate_ml(double prior,
				      const FeatureVec &f)
{
}


// FIXME: NOT TRIED
void
SubspaceConstrainedGaussian::accumulate_mmi_denominator(std::vector<double> priors,
						   std::vector<const FeatureVec*> const features)
{
}


// FIXME: NOT TRIED
void
SubspaceConstrainedGaussian::estimate_parameters()
{
}


// FIXME: NOT TRIED
void
SubspaceConstrainedGaussian::get_mean(Vector &mean) const
{
}


// FIXME: NOT TRIED
void
SubspaceConstrainedGaussian::get_covariance(Matrix &covariance) const
{
}


// FIXME: NOT TRIED
void
SubspaceConstrainedGaussian::set_mean(const Vector &mean)
{
}


// FIXME: NOT TRIED
void
SubspaceConstrainedGaussian::set_covariance(const Matrix &covariance)
{
}


Mixture::Mixture()
{
}


Mixture::Mixture(PDFPool *pool)
{
  m_pool = pool;
}


Mixture::~Mixture()
{
}


void
Mixture::reset()
{
  m_weights.resize(0);
  m_pointers.resize(0);
}


void
Mixture::set_pool(PDFPool *pool)
{
  m_pool = pool;
}


void
Mixture::set_components(const std::vector<int> &pointers,
			const std::vector<double> &weights)
{
  assert(pointers.size()==weights.size());

  m_pointers.resize(pointers.size());
  m_weights.resize(weights.size());

  for (unsigned int i=0; i<pointers.size(); i++) {
    m_pointers[i] = pointers[i];
    m_weights[i] = weights[i];
  }
}


void
Mixture::get_components(std::vector<int> &pointers,
			std::vector<double> &weights)
{
  pointers.resize(m_pointers.size());
  weights.resize(m_weights.size());
  
  for (unsigned int i=0; i<m_pointers.size(); i++) {
    pointers[i] = m_pointers[i];
    weights[i] = m_weights[i];
  }
}


void
Mixture::add_component(int pool_index,
		       double weight)
{
  assert(m_weights.size() == m_pointers.size());
  m_pointers.push_back(pool_index);
  m_weights.push_back(weight);
}


void
Mixture::normalize_weights()
{
  double sum=0;
  for (unsigned int i=0; i<m_weights.size(); i++)
    sum += m_weights[i];
  for (unsigned int i=0; i<m_weights.size(); i++)
    m_weights[i] /= sum;
}


double
Mixture::compute_likelihood(const FeatureVec &f) const
{
  m_pool->cache_likelihood(f, m_pointers);
  return compute_likelihood();
}


double
Mixture::compute_log_likelihood(const FeatureVec &f) const
{
  m_pool->cache_likelihood(f, m_pointers);
  return compute_log_likelihood();
}


double
Mixture::compute_likelihood() const
{
  double ll = 0;
  for (unsigned int i=0; i< m_pointers.size(); i++) {
    ll += m_weights[i]*m_pool->get_likelihood(m_pointers[i]);
  }
  return ll;
}


double
Mixture::compute_log_likelihood() const
{
  double ll = 0;
  for (unsigned int i=0; i< m_pointers.size(); i++) {
    ll += m_weights[i]*m_pool->get_likelihood(m_pointers[i]);
  }
  return util::safe_log(ll);
}


void
Mixture::write(std::ostream &os) const
{
  os << m_pointers.size();
  for (unsigned int w = 0; w < m_pointers.size(); w++) {
    os << " " << m_pointers[w]
	<< " " << m_weights[w];
  }
  os << std::endl;
}


void
Mixture::read(std::istream &is)
{
  int num_weights = 0;
  is >> num_weights;
  
  int index;
  double weight;
  for (int w = 0; w < num_weights; w++) {
    is >> index >> weight;
    add_component(index, weight);
  }
  normalize_weights();
}


PDFPool::~PDFPool() {
  for (unsigned int i=0; i<m_pool.size(); i++)
    delete m_pool[i];
}


void
PDFPool::reset() {
  m_dim=0;
  m_pool.clear();
  m_likelihoods.clear();
}


PDF*
PDFPool::get_pdf(int index)
{
  return m_pool[index];
}


void
PDFPool::set_pdf(int pdfindex, PDF *pdf)
{  
}


void
PDFPool::reset_cache()
{
  for (unsigned int i=0; i<m_likelihoods.size(); i++)
    m_likelihoods[i] = -1;
}


void
PDFPool::cache_likelihood(const FeatureVec &f)

{
  for (unsigned int i=0; i<m_likelihoods.size(); i++)
    m_likelihoods[i] = m_pool[i]->compute_likelihood(f);
}


void
PDFPool::cache_likelihood(const FeatureVec &f,
			  int index)
{
  m_likelihoods[index] = m_pool[index]->compute_likelihood(f);
}


void
PDFPool::cache_likelihood(const FeatureVec &f,
			  const std::vector<int> &indices)
{
  for (unsigned int i=0; i<indices.size(); i++)
    m_likelihoods[indices[i]] = m_pool[indices[i]]->compute_likelihood(f);
}


double
PDFPool::get_likelihood(int index) const
{
  return m_likelihoods[index];
}


void
PDFPool::read_gk(const std::string &filename)
{
  std::ifstream in(filename.c_str());
  if (!in)
    throw std::string("PDFPool::read_gk(): could not open %s\n");
  
  int pdfs = 0;
  std::string type_str;
  in >> pdfs >> m_dim >> type_str;
  m_pool.resize(pdfs);
  m_likelihoods.resize(pdfs);

  // New implementation
  if (type_str == "variable") {
    for (unsigned int i=0; i<m_pool.size(); i++) {
      in >> type_str;

      if (type_str == "diagonal_cov") {
	m_pool[i]=new DiagonalGaussian(m_dim);
      }
      else if (type_str == "full_cov") {
	m_pool[i]=new FullCovarianceGaussian(m_dim);
      }
      else if (type_str == "pcgmm") {
	m_pool[i]=new PrecisionConstrainedGaussian(m_dim);
      }
      else if (type_str == "scgmm") {
	m_pool[i]=new SubspaceConstrainedGaussian(m_dim);
      }
    }
  }

  // For compliance
  else {
    if (type_str == "diagonal_cov") {
      for (unsigned int i=0; i<m_pool.size(); i++) {
	m_pool[i]=new DiagonalGaussian(m_dim);
	m_pool[i]->read(in);
      }
    }
    else if (type_str == "full_cov") {
      for (unsigned int i=0; i<m_pool.size(); i++) {
	m_pool[i]=new FullCovarianceGaussian(m_dim);
	m_pool[i]->read(in);
      }      
    }
    else if (type_str == "pcgmm") {
      for (unsigned int i=0; i<m_pool.size(); i++) {
	m_pool[i]=new PrecisionConstrainedGaussian(m_dim);
	m_pool[i]->read(in);
      }      
    }
    else if (type_str == "scgmm") {
      for (unsigned int i=0; i<m_pool.size(); i++) {
	m_pool[i]=new SubspaceConstrainedGaussian(m_dim);
	m_pool[i]->read(in);
      }            
    }
    else
      throw std::string("Unknown model type\n");
  }
  
  if (!in)
    throw std::string("PDFPool::read_gk(): error reading file: %s\n");
}


void
PDFPool::write_gk(const std::string &filename) const
{
  std::ofstream out(filename.c_str());
  
  out << m_pool.size() << " " << m_dim << " variable\n";

  for (unsigned int i=0; i<m_pool.size(); i++)
    m_pool[i]->write(out);
}