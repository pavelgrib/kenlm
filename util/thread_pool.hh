#ifndef UTIL_THREAD_POOL_H
#define UTIL_THREAD_POOL_H

#include "util/pcqueue.hh"

#include <optional>
#include <boost/thread.hpp>

#include <iostream>
#include <cstdlib>

namespace util {

template <class HandlerT> class Worker {
  public:
    typedef HandlerT Handler;
    typedef typename Handler::Request Request;

    Worker(const Worker&) = delete;
    Worker& opereator=(const Worker&) = delete;

    template <class Construct> Worker(PCQueue<Request> &in, Construct &construct, const Request &poison)
      : in_(in), handler_(construct), poison_(poison), thread_(std::ref(*this)) {}

    // Only call from thread.
    void operator()() {
      Request request;
      while (1) {
        in_.Consume(request);
        if (request == poison_) return;
        try {
          (*handler_)(request);
        }
        catch(const std::exception &e) {
          std::cerr << "Handler threw " << e.what() << std::endl;
          abort();
        }
        catch(...) {
          std::cerr << "Handler threw an exception, dropping request" << std::endl;
          abort();
        }
      }
    }

    void Join() {
      thread_.join();
    }

  private:
    PCQueue<Request> &in_;

    std::optional<Handler> handler_;

    const Request poison_;

    std::thread thread_;
};

template <class HandlerT> class ThreadPool {
  public:
    typedef HandlerT Handler;
    typedef typename Handler::Request Request;

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& opereator=(const ThreadPool&) = delete;

    template <class Construct> ThreadPool(std::size_t queue_length, std::size_t workers, Construct handler_construct, Request poison) : in_(queue_length), poison_(poison) {
      for (size_t i = 0; i < workers; ++i) {
        workers_.push_back(new Worker<Handler>(in_, handler_construct, poison));
      }
    }

    ~ThreadPool() {
      for (std::size_t i = 0; i < workers_.size(); ++i) {
        Produce(poison_);
      }

      for (auto i: workers_) {
        i->Join();
      }
    }

    void Produce(const Request &request) {
      in_.Produce(request);
    }

    // For adding to the queue.
    PCQueue<Request> &In() { return in_; }

  private:
    PCQueue<Request> in_;

    std::vector<std::unique_ptr<Handler>> > workers_;

    Request poison_;
};

template <class Handler> class RecyclingHandler {
  public:
    typedef typename Handler::Request Request;

    template <class Construct> RecyclingHandler(PCQueue<Request> &recycling, Construct &handler_construct)
      : inner_(handler_construct), recycling_(recycling) {}

    void operator()(Request &request) {
      inner_(request);
      recycling_.Produce(request);
    }

  private:
    Handler inner_;
    PCQueue<Request> &recycling_;
};

template <class HandlerT> class RecyclingThreadPool {
  public:
    typedef HandlerT Handler;
    typedef typename Handler::Request Request;

    RecyclingThreadPool(const RecyclingThreadPool&) = delete;
    RecyclingThreadPool& opereator=(const RecyclingThreadPool&) = delete;

    // Remember to call PopulateRecycling afterwards in most cases.
    template <class Construct> RecyclingThreadPool(std::size_t queue, std::size_t workers, Construct handler_construct, Request poison)
      : recycling_(queue), pool_(queue, workers, RecyclingHandler<Handler>(recycling_, handler_construct), poison) {}

    // Initialization: put stuff into the recycling queue.  This could also be
    // done by calling Produce without Consume, but it's often easier to
    // initialize with PopulateRecycling then do a Consume/Produce loop.
    void PopulateRecycling(const Request &request) {
      recycling_.Produce(request);
    }

    Request Consume() {
      return recycling_.Consume();
    }

    void Produce(const Request &request) {
      pool_.Produce(request);
    }
    
  private:
    PCQueue<Request> recycling_;
    ThreadPool<RecyclingHandler<Handler> > pool_;
};

} // namespace util

#endif // UTIL_THREAD_POOL_H
