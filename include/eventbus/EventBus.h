/*
 * Copyright (c) 2020, Dan Quist
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "EventHandler.h"
#include "RoutedEvent.h"
#include "HandlerRegistration.h"

#include <list>
#include <unordered_map>
#include <typeindex>
#include "HandlerCollectionMap.h"


 /**
	* \brief An Event system that allows decoupling of code through synchronous events
	*
	*/
class EventBus final
{
public:
	/**
	 * \brief Default empty constructor
	 */
	EventBus() {}


	/**
	 * \brief Empty virtual destructor
	 */
	virtual ~EventBus() {}


	/**
	 * \brief Registers a new event handler to the EventBus with a source specifier
	 *
	 * The template parameter is the specific type of event that is being added. Since a class can
	 * potentially inherit multiple event handlers, the template specifier will remove any ambiguity
	 * as to which handler pointer is being referenced.
	 *
	 * @param handler The event handler class
	 * @param sender The source sender object
	 * @return An EventRegistration pointer which can be used to unregister the event handler
	 */
	template <class T>
	HandlerRegistration* const AddHandler(EventHandler<T>& handler, void* sender)
	{
		// Fetch the list of event pairs unique to this event type
		Registrations* registrations = this->handlers[typeid(T)];

		// Create a new collection instance for this type if it hasn't been created yet
		if (registrations == nullptr)
		{
			registrations = new Registrations();
			this->handlers[typeid(T)] = registrations;
		}

		// Create a new EventPair instance for this registration.
		// This will group the handler, sender, and registration object into the same class
		EventRegistration* registration = new EventRegistration(static_cast<void*>(&handler), registrations, &sender);

		// Add the registration object to the collection
		registrations->push_back(registration);

		return registration;
	}


	/**
	 * \brief Registers a new event handler to the EventBus with no source specified
	 *
	 * @param handler The event handler class
	 * @return An EventRegistration pointer which can be used to unregister the event handler
	 */
	template <class T>
	HandlerRegistration* const AddHandler(EventHandler<T>& handler)
	{
		// Fetch the list of event pairs unique to this event type
		Registrations* registrations = this->handlers[typeid(T)];

		// Create a new collection instance for this type if it hasn't been created yet
		if (registrations == nullptr)
		{
			registrations = new Registrations();
			this->handlers[typeid(T)] = registrations;
		}

		// Create a new EventPair instance for this registration.
		// This will group the handler, sender, and registration object into the same class
		EventRegistration* registration = new EventRegistration(static_cast<void*>(&handler), registrations, nullptr);

		// Add the registration object to the collection
		registrations->push_back(registration);

		return registration;
	}


	/**
	 * \brief Fires an event
	 *
	 * @param e The event to fire
	 */
	void FireEvent(RoutedEvent& e)
	{
		Registrations* registrations = this->handlers[typeid(e)];

		// If the registrations list is null, then no handlers have been registered for this event
		if (registrations == nullptr)
		{
			return;
		}

		// Iterate through all the registered handlers and dispatch to each one if the sender
		// matches the source or if the sender is not specified
		for (auto& reg : *registrations)
		{
			// This is where some magic happens. The void * handler is statically cast to an event handler
			// of generic type Event and dispatched. The dispatch function will then do a dynamic
			// cast to the correct event type so the matching onEvent method can be called
			static_cast<EventHandler<RoutedEvent>*>(reg->getHandler())->dispatch(e);
		}
	}

	EventSubscription Add(const SubscriptionDescriptor& descriptor)
	{
		return _collectionMap.Add(descriptor);
	}

	template <typename TEvent>
	void Publish(TEvent& event)
	{
		_collectionMap.Dispatch(event);
	}

	template <typename TEvent>
	EventSubscription Subscribe(std::function<void(TEvent&)> handler, std::function<bool(TEvent&)> predicate)
	{
		const std::function<void(RoutedEvent&)> internalHandler = [=](RoutedEvent& e)
		{
			handler(dynamic_cast<TEvent&>(e));
		};

		const std::function<bool(RoutedEvent&)> internalPredicate = [=](RoutedEvent& e)
		{
			return predicate(dynamic_cast<TEvent&>(e));
		};

		const SubscriptionDescriptor descriptor(typeid(TEvent), internalHandler, internalPredicate);
		return Add(descriptor);
	}

	template <typename TEvent>
	EventSubscription Subscribe(std::function<void(TEvent&)> handler)
	{
		return Subscribe<TEvent>(handler, [](TEvent&) { return true; });
	}


private:
	/**
	 * \brief Registration class private to EventBus for registered event handlers
	 */
	class EventRegistration : public HandlerRegistration
	{
	public:
		typedef std::list<EventRegistration*> Registrations;


		/**
		 * \brief Represents a registration object for a registered event handler
		 *
		 * This object is stored in a collection with other handlers for the event type.
		 *
		 * @param handler The event handler
		 * @param registrations The handler collection for this event type
		 * @param sender The registered sender object
		 */
		EventRegistration(void* const handler, Registrations* const registrations, void* const sender) :
			handler(handler),
			registrations(registrations),
			sender(sender),
			registered(true)
		{
		}


		/**
		 * \brief Empty virtual destructor
		 */
		virtual ~EventRegistration() {}


		/**
		 * \brief Gets the event handler for this registration
		 *
		 * @return The event handler
		 */
		void* const getHandler()
		{
			return handler;
		}


		/**
		 * \brief Gets the sender object for this registration
		 *
		 * @return The registered sender object
		 */
		void* const getSender()
		{
			return sender;
		}


		/**
		 * \brief Removes an event handler from the registration collection
		 *
		 * The event handler will no longer receive events for this event type
		 */
		virtual void removeHandler()
		{
			if (registered)
			{
				registrations->remove(this);
				registered = false;
			}
		}

	private:
		void* const handler;
		Registrations* const registrations;
		void* const sender;

		bool registered;
	};

	typedef std::list<EventRegistration*> Registrations;
	typedef std::unordered_map<std::type_index, std::list<EventRegistration*>*> TypeMap;

	TypeMap handlers;
	std::unordered_map<std::type_index, HandlerCollection*> _map;

	HandlerCollectionMap _collectionMap;

};
